#include "DebugServer.hpp"

// The threading rule this whole file exists to enforce:
//
//   The HTTP thread never touches engine state.
//
// Every Pine subsystem is global mutable state in an anonymous namespace with no locking, and
// OpenGL is main-thread-only. httplib runs each handler on its own thread. So an HTTP worker only
// parses its request, hands a job to the main thread, blocks, and serializes whatever comes back.
//
// Pine's own Threading system is not usable for this: PumpMainThreadTasks() is only ever called
// from inside AwaitTaskResult/AwaitTaskPool, and Engine::Run() never pumps, so a background thread
// queueing a MainThread task and awaiting it would wait forever. Hence the small queue below,
// drained once per frame from a render callback.

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include <httplib.h>

#include "Pine/Core/Log/Log.hpp"
#include "Pine/Rendering/RenderManager/RenderManager.hpp"

#include "Endpoints/Endpoints.hpp"

namespace
{
    // Loopback only, deliberately: this is an unauthenticated control channel into a running editor.
    constexpr const char* m_ListenAddress = "127.0.0.1";
    constexpr int m_DefaultPort = 9002;

    // How long an HTTP thread waits for the main thread to pick its request up. The editor can
    // legitimately stall - a modal dialog, a long asset import - and a debug tool that hangs a
    // client forever is worse than one that admits it timed out.
    constexpr auto m_RequestTimeout = std::chrono::seconds(5);

    // One in-flight request, handed from an HTTP worker to the main thread and back. Held by
    // shared_ptr because on timeout the worker walks away while the main thread may still own it.
    struct PendingRequest
    {
        Editor::DebugServer::Handler Invoke;
        std::string Path;

        Editor::DebugServer::Request RequestData;
        Editor::DebugServer::Response ResponseData;

        bool Completed = false;
    };

    httplib::Server m_Server;
    std::thread m_ListenerThread;

    // Guards m_RequestQueue and every PendingRequest::Completed flag.
    std::mutex m_QueueMutex;
    std::condition_variable m_CompletionSignal;

    std::vector<std::shared_ptr<PendingRequest>> m_RequestQueue;

    bool m_Running = false;

    // PINE_DEBUG_SERVER both enables the server and picks its port, so there is one knob rather than
    // two: unset means off, "1" means on at the default port, "9100" means on at 9100.
    std::optional<int> ResolveListenPort()
    {
        const char* configuration = std::getenv("PINE_DEBUG_SERVER");

        if (configuration == nullptr)
        {
            return std::nullopt;
        }

        const int port = std::atoi(configuration);

        // Anything that isn't a usable port number - including the "1" that just means "on" - falls
        // back to the default rather than refusing to start.
        if (port < 1024 || port > 65535)
        {
            return m_DefaultPort;
        }

        return port;
    }

    void WriteResponse(httplib::Response& response, const Editor::DebugServer::Response& result)
    {
        response.status = result.StatusCode;

        if (!result.Binary.empty())
        {
            response.set_content(reinterpret_cast<const char*>(result.Binary.data()),
                                 result.Binary.size(),
                                 result.ContentType);

            return;
        }

        // Pretty-printed on purpose. The consumer here is a person or an agent reading curl output,
        // not a browser, and the bandwidth is a loopback socket.
        //
        // The replace error handler matters: entity names and asset paths come out of level files as
        // raw bytes, and dump() throws on invalid UTF-8 by default. That would throw here, on the
        // HTTP thread, outside the handler's own try/catch.
        response.set_content(result.Body.dump(2, ' ', false, nlohmann::json::error_handler_t::replace),
                             "application/json");
    }

    Editor::DebugServer::Response InvokeHandler(const PendingRequest& pending)
    {
        try
        {
            return pending.Invoke(pending.RequestData);
        }
        catch (const std::exception& exception)
        {
            // A debug endpoint must never take the editor down with it.
            PError(fmt::format("Debug server endpoint '{}' threw: {}", pending.Path, exception.what()));

            return Editor::DebugServer::Error(500, exception.what());
        }
    }

    void DrainRequestQueue()
    {
        std::vector<std::shared_ptr<PendingRequest>> batch;

        {
            std::lock_guard lock(m_QueueMutex);

            if (m_RequestQueue.empty())
            {
                return;
            }

            batch.swap(m_RequestQueue);
        }

        // Handlers run outside the lock. They may be slow, and holding the queue mutex here would
        // stall every other HTTP worker trying to queue its own request.
        for (const auto& pending : batch)
        {
            pending->ResponseData = InvokeHandler(*pending);
        }

        {
            std::lock_guard lock(m_QueueMutex);

            for (const auto& pending : batch)
            {
                pending->Completed = true;
            }
        }

        m_CompletionSignal.notify_all();
    }

    void OnPineRender(Pine::RenderingContext*, const Pine::RenderStage stage, float)
    {
        // PostRender fires once per frame with a null context, after every rendering context is
        // done - so engine state is settled and nothing is halfway through a pass.
        if (stage != Pine::RenderStage::PostRender)
        {
            return;
        }

        DrainRequestQueue();
    }

    void DispatchToMainThread(const std::string& path,
                              const Editor::DebugServer::Handler& handler,
                              const httplib::Request& request,
                              httplib::Response& response)
    {
        auto pending = std::make_shared<PendingRequest>();

        pending->Invoke = handler;
        pending->Path = path;
        pending->RequestData.Body = request.body;

        for (const auto& [name, value] : request.params)
        {
            pending->RequestData.Parameters[name] = value;
        }

        std::unique_lock lock(m_QueueMutex);

        if (!m_Running)
        {
            lock.unlock();

            WriteResponse(response, Editor::DebugServer::Error(503, "Debug server is shutting down."));

            return;
        }

        m_RequestQueue.push_back(pending);

        m_CompletionSignal.wait_for(lock, m_RequestTimeout, [&]
        {
            return pending->Completed || !m_Running;
        });

        if (!pending->Completed)
        {
            const bool shuttingDown = !m_Running;

            lock.unlock();

            // The main thread may still be holding this request, so nothing here touches
            // ResponseData. It will be filled in for nobody and dropped, which is harmless.
            WriteResponse(response, shuttingDown
                ? Editor::DebugServer::Error(503, "Debug server is shutting down.")
                : Editor::DebugServer::Error(504, "Timed out waiting for the editor's main thread."));

            return;
        }

        // The main thread wrote ResponseData before setting Completed under this same mutex, so the
        // value is visible here.
        const auto result = pending->ResponseData;

        lock.unlock();

        WriteResponse(response, result);
    }
}

void Editor::DebugServer::AddRoute(const Method method, const std::string& path, Handler handler)
{
    auto dispatch = [path, handler](const httplib::Request& request, httplib::Response& response)
    {
        DispatchToMainThread(path, handler, request, response);
    };

    switch (method)
    {
    case Method::Get:
        m_Server.Get(path, dispatch);
        break;
    case Method::Post:
        m_Server.Post(path, dispatch);
        break;
    }
}

Editor::DebugServer::Response Editor::DebugServer::Error(const int statusCode, const std::string& message)
{
    nlohmann::json body;

    body["error"] = message;

    return { statusCode, body };
}

void Editor::DebugServer::Setup()
{
    const auto port = ResolveListenPort();

    if (!port.has_value())
    {
        return;
    }

    m_Running = true;

    Endpoints::Register();

    Pine::RenderManager::AddRenderCallback(OnPineRender);

    m_ListenerThread = std::thread([listenPort = *port]
    {
        if (!m_Server.listen(m_ListenAddress, listenPort))
        {
            PError(fmt::format("Debug server failed to listen on {}:{}.", m_ListenAddress, listenPort));
        }
    });

    // Safe against a failed bind: httplib marks itself decommissioned when it cannot take the
    // port, so this returns rather than spinning, and the editor still boots.
    m_Server.wait_until_ready();

    if (!m_Server.is_running())
    {
        // listen() has already logged why. Join the finished thread here rather than going through
        // Shutdown(), which would report a stop that never happened.
        m_ListenerThread.join();

        m_Running = false;

        return;
    }

    PInfo(fmt::format("Debug server listening on http://{}:{}", m_ListenAddress, *port));
}

void Editor::DebugServer::Shutdown()
{
    if (!m_ListenerThread.joinable())
    {
        return;
    }

    // Order matters here. httplib's stop() waits for in-flight handlers to return, and those
    // handlers are parked on m_CompletionSignal waiting for a main thread that will never drain the
    // queue again. Release them first, or stop() deadlocks against them.
    {
        std::lock_guard lock(m_QueueMutex);

        m_Running = false;
        m_RequestQueue.clear();
    }

    m_CompletionSignal.notify_all();

    m_Server.stop();

    m_ListenerThread.join();

    PInfo("Debug server stopped.");
}
