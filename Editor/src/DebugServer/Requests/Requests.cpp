#include "Requests.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Core/UId/UId.hpp"

namespace
{
    using namespace Editor::DebugServer;
    using Clock = std::chrono::steady_clock;

    constexpr auto m_RequestTimeout = std::chrono::seconds(5);
    constexpr auto m_Retention = std::chrono::minutes(10);
    constexpr std::size_t m_MaxRetainedRequests = 256;
    constexpr std::size_t m_MaxActiveRequests = 256;
    constexpr std::size_t m_MaxMutationBytes = 256 * 1024;

    enum class State
    {
        Pending,
        Running,
        Succeeded,
        Rejected,
        Failed,
        Cancelled
    };

    struct PendingRequest
    {
        Handler Invoke;
        std::string Path;
        Request Input;
        std::string Identity;
        bool Mutation = false;

        State Status = State::Pending;
        Response Result;
        Clock::time_point Deadline;
        Clock::time_point FinishedAt;

        // Owned by the main thread, separate from the result read by HTTP workers.
        std::function<Response()> Resume;
    };

    std::string m_Session;
    bool m_Running = false;
    std::size_t m_ActiveCount = 0;
    std::mutex m_Mutex;
    std::condition_variable m_CompletionSignal;
    std::vector<std::shared_ptr<PendingRequest>> m_Queue;
    std::unordered_map<std::string, std::shared_ptr<PendingRequest>> m_Records;

    // Only the main thread accesses the deferred list.
    std::vector<std::shared_ptr<PendingRequest>> m_Deferred;

    bool IsFinished(State state)
    {
        return state != State::Pending && state != State::Running;
    }

    const char* StateName(State state)
    {
        switch (state)
        {
        case State::Pending: return "pending";
        case State::Running: return "running";
        case State::Succeeded: return "succeeded";
        case State::Rejected: return "rejected";
        case State::Failed: return "failed";
        case State::Cancelled: return "cancelled";
        }
        return "unknown";
    }

    nlohmann::json Describe(const PendingRequest& pending)
    {
        return {
            { "id", pending.Identity }, { "session", m_Session },
            { "state", StateName(pending.Status) },
            { "tracked", !pending.Identity.empty() },
            { "mayHaveExecuted", pending.Status == State::Running ||
                pending.Status == State::Succeeded || pending.Status == State::Failed }
        };
    }

    Response Reject(int code, const std::string& message)
    {
        auto result = Error(code, message);
        result.Body["request"] = {
            { "state", "rejected" }, { "tracked", false }, { "mayHaveExecuted", false }
        };
        return result;
    }

    // All state transitions and result publication take place under m_Mutex.
    void Finish(PendingRequest& pending, State state, Response result)
    {
        pending.Status = state;
        pending.FinishedAt = Clock::now();
        if (pending.Mutation)
        {
            result.Body["request"] = Describe(pending);
        }
        pending.Result = std::move(result);
        --m_ActiveCount;
        m_CompletionSignal.notify_all();
    }

    void CancelPending(PendingRequest& pending, int code, const std::string& message)
    {
        Finish(pending, State::Cancelled, Error(code, message));
    }

    void ExpireRecords()
    {
        const auto now = Clock::now();
        for (auto it = m_Records.begin(); it != m_Records.end();)
        {
            const auto& pending = it->second;
            if (IsFinished(pending->Status) && now - pending->FinishedAt >= m_Retention)
            {
                it = m_Records.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void ExpireQueuedRequests()
    {
        const auto now = Clock::now();
        for (const auto& pending : m_Queue)
        {
            if (pending->Status == State::Pending && now >= pending->Deadline)
            {
                CancelPending(*pending, 504, "Request expired before execution; no operation ran.");
            }
        }
        m_Queue.erase(std::remove_if(m_Queue.begin(), m_Queue.end(), [](const auto& pending)
        {
            return IsFinished(pending->Status);
        }), m_Queue.end());
    }

    bool ValidIdentity(const std::string& identity)
    {
        return !identity.empty() && identity.size() <= 128 &&
            std::all_of(identity.begin(), identity.end(), [](unsigned char c)
            {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
            });
    }

    Response Unknown(const std::string& identity)
    {
        auto result = Error(404, "Request is unknown or its retained result has expired. Execution cannot be inferred.");
        result.Body["request"] = {
            { "id", identity }, { "session", m_Session }, { "state", "unknown" },
            { "tracked", false }, { "mayHaveExecuted", true }
        };
        return result;
    }

    Response UnknownSession(const std::string& identity, const std::string& session)
    {
        auto result = Unknown(identity);
        result.StatusCode = 409;
        result.Body["error"] = "Request belongs to a different server session; its outcome is unknown here.";
        result.Body["request"]["session"] = session;
        result.Body["currentSession"] = m_Session;
        return result;
    }

    Response Snapshot(const PendingRequest& pending)
    {
        nlohmann::json body = { { "request", Describe(pending) } };
        if (IsFinished(pending.Status))
        {
            body["result"] = {
                { "status", pending.Result.StatusCode }, { "body", pending.Result.Body }
            };
        }
        return { 200, body };
    }

    void Execute(const std::shared_ptr<PendingRequest>& pending)
    {
        Response result;
        try
        {
            result = pending->Resume ? pending->Resume() : pending->Invoke(pending->Input);
        }
        catch (const std::exception& exception)
        {
            PError(fmt::format("Debug server endpoint '{}' threw: {}", pending->Path, exception.what()));
            result = Error(500, exception.what());
        }

        std::lock_guard lock(m_Mutex);
        if (IsFinished(pending->Status))
        {
            // A timed-out deferred read may have finished while its worker cancelled it.
            return;
        }

        if (result.Resume && !pending->Mutation)
        {
            pending->Resume = std::move(result.Resume);
            m_Deferred.push_back(pending);
            return;
        }
        if (result.Resume)
        {
            result = Error(500, "Mutating handlers must finish synchronously on the main thread.");
        }

        State state = State::Succeeded;
        if (result.StatusCode >= 500)
        {
            state = State::Failed;
        }
        else if (result.StatusCode >= 400)
        {
            state = State::Rejected;
        }
        Finish(*pending, state, std::move(result));
    }
}

void Editor::DebugServer::Requests::Setup()
{
    m_Session = Pine::UId::New().ToString();
    m_Running = true;
}

const std::string& Editor::DebugServer::Requests::GetSession()
{
    return m_Session;
}

Editor::DebugServer::Response Editor::DebugServer::Requests::Dispatch(
    const std::string& path, Handler handler, Request request,
    bool mutation, const std::string& identity, const std::string& session)
{
    if ((!identity.empty() || !session.empty()) && !mutation)
    {
        return Reject(400, "Retry headers are supported only on mutation endpoints.");
    }
    if ((!identity.empty() || !session.empty()) && (!ValidIdentity(identity) || session.empty()))
    {
        return Reject(400, "Provide Idempotency-Key (1-128 letters, digits, '.', '_' or '-') and X-Pine-Session.");
    }
    if (mutation && request.Body.size() > m_MaxMutationBytes)
    {
        return Reject(413, "Mutation body exceeds 256 KiB.");
    }

    std::unique_lock lock(m_Mutex);
    if (!m_Running)
    {
        return Reject(503, "Debug server is shutting down.");
    }
    if (!identity.empty() && session != m_Session)
    {
        return Reject(409, "Request belongs to a different server session.");
    }

    ExpireRecords();
    ExpireQueuedRequests();
    std::shared_ptr<PendingRequest> pending;
    if (const auto found = m_Records.find(identity); found != m_Records.end())
    {
        pending = found->second;
        if (pending->Path != path || pending->Input.Body != request.Body ||
            pending->Input.Parameters != request.Parameters)
        {
            return Reject(409, "Idempotency-Key was already used with a different path, query or body.");
        }
    }
    else
    {
        if (m_ActiveCount >= m_MaxActiveRequests ||
            (!identity.empty() && m_Records.size() >= m_MaxRetainedRequests))
        {
            return Reject(503, "Request capacity reached. No request was admitted; retry later.");
        }

        pending = std::make_shared<PendingRequest>();
        pending->Invoke = std::move(handler);
        pending->Path = path;
        pending->Input = std::move(request);
        pending->Identity = identity;
        pending->Mutation = mutation;
        pending->Deadline = Clock::now() + m_RequestTimeout;
        ++m_ActiveCount;
        m_Queue.push_back(pending);
        if (!identity.empty())
        {
            m_Records.emplace(identity, pending);
        }
    }

    m_CompletionSignal.wait_until(lock, pending->Deadline, [&]
    {
        return IsFinished(pending->Status) || !m_Running;
    });

    if (pending->Status == State::Pending)
    {
        CancelPending(*pending, 504, "Request expired before execution; no operation ran.");
        ExpireQueuedRequests();
    }
    else if (pending->Status == State::Running && !mutation)
    {
        CancelPending(*pending, 504, "Timed out waiting for the editor; read discarded.");
    }

    if (IsFinished(pending->Status))
    {
        return pending->Result;
    }

    const auto message = identity.empty()
        ? "Request has started and may have executed. No retry identity was supplied; inspect scene state before retrying."
        : "Request has started and may have executed. It cannot be cancelled; inspect request status.";
    auto result = Error(504, message);
    result.Body["request"] = Describe(*pending);
    return result;
}

Editor::DebugServer::Response Editor::DebugServer::Requests::Status(
    const std::string& identity, const std::string& session)
{
    std::lock_guard lock(m_Mutex);
    if (identity.empty())
    {
        return { 200, {
            { "session", m_Session }, { "retentionSeconds", std::chrono::seconds(m_Retention).count() },
            { "maxRetainedRequests", m_MaxRetainedRequests }, { "maxActiveRequests", m_MaxActiveRequests },
            { "timeoutSeconds", m_RequestTimeout.count() }
        } };
    }
    if (!ValidIdentity(identity) || session.empty())
    {
        return Reject(400, "Request status requires a valid id and X-Pine-Session.");
    }
    if (session != m_Session)
    {
        return UnknownSession(identity, session);
    }

    ExpireRecords();
    ExpireQueuedRequests();
    const auto found = m_Records.find(identity);
    return found == m_Records.end() ? Unknown(identity) : Snapshot(*found->second);
}

Editor::DebugServer::Response Editor::DebugServer::Requests::Cancel(
    const std::string& identity, const std::string& session)
{
    std::lock_guard lock(m_Mutex);
    if (!ValidIdentity(identity) || session.empty())
    {
        return Reject(400, "Cancellation requires a valid id and X-Pine-Session.");
    }
    if (session != m_Session)
    {
        return UnknownSession(identity, session);
    }

    ExpireRecords();
    ExpireQueuedRequests();
    const auto found = m_Records.find(identity);
    if (found == m_Records.end())
    {
        return Unknown(identity);
    }
    auto& pending = *found->second;
    if (pending.Status == State::Running)
    {
        auto result = Snapshot(pending);
        result.StatusCode = 409;
        result.Body["error"] = "Request has started and cannot be cancelled.";
        return result;
    }
    if (pending.Status == State::Pending)
    {
        CancelPending(pending, 409, "Request cancelled before execution; no operation ran.");
        ExpireQueuedRequests();
    }
    return Snapshot(pending);
}

void Editor::DebugServer::Requests::Drain()
{
    std::vector<std::shared_ptr<PendingRequest>> batch;
    {
        std::lock_guard lock(m_Mutex);
        ExpireQueuedRequests();
        batch.swap(m_Queue);
    }

    for (const auto& pending : batch)
    {
        {
            std::lock_guard lock(m_Mutex);
            if (IsFinished(pending->Status))
            {
                continue;
            }
            if (Clock::now() >= pending->Deadline)
            {
                CancelPending(*pending, 504, "Request expired before execution; no operation ran.");
                continue;
            }
            pending->Status = State::Running;
        }
        Execute(pending);
    }
}

void Editor::DebugServer::Requests::ResumeReads()
{
    std::vector<std::shared_ptr<PendingRequest>> batch;
    batch.swap(m_Deferred);
    for (const auto& pending : batch)
    {
        {
            std::lock_guard lock(m_Mutex);
            if (IsFinished(pending->Status))
            {
                continue;
            }
            if (Clock::now() >= pending->Deadline)
            {
                CancelPending(*pending, 504, "Timed out waiting for the editor; read discarded.");
                continue;
            }
        }
        Execute(pending);
    }
}

void Editor::DebugServer::Requests::Shutdown()
{
    // Called on the main thread, so no mutation handler is executing concurrently.
    std::lock_guard lock(m_Mutex);
    m_Running = false;
    for (const auto& pending : m_Queue)
    {
        if (!IsFinished(pending->Status))
        {
            CancelPending(*pending, 503, "Debug server shut down before execution.");
        }
    }
    for (const auto& pending : m_Deferred)
    {
        if (!IsFinished(pending->Status))
        {
            CancelPending(*pending, 503, "Debug server shut down before the read completed.");
        }
    }
    m_Queue.clear();
    m_Deferred.clear();
    m_Records.clear();
    m_CompletionSignal.notify_all();
}
