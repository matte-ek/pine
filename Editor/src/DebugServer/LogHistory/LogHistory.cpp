#include "LogHistory.hpp"

#include <charconv>
#include <optional>

#include "Pine/Core/Log/Log.hpp"

namespace
{
    const char* SeverityName(Pine::LogSeverity severity)
    {
        switch (severity)
        {
        case Pine::LogSeverity::Verbose:
            return "verbose";
        case Pine::LogSeverity::Info:
            return "info";
        case Pine::LogSeverity::Warning:
            return "warning";
        case Pine::LogSeverity::Error:
            return "error";
        case Pine::LogSeverity::Fatal:
            return "fatal";
        }
        return "unknown";
    }

    bool ReadNumber(const std::string& value, std::uint64_t& result)
    {
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
        return parsed.ec == std::errc() && parsed.ptr == value.data() + value.size();
    }
}

Editor::DebugServer::Response Editor::DebugServer::LogHistory::Get(const Request& request)
{
    std::uint64_t limit = 256;
    std::optional<std::uint64_t> since;

    if (const auto parameter = request.Parameters.find("limit"); parameter != request.Parameters.end())
    {
        if (!ReadNumber(parameter->second, limit) || limit < 1)
        {
            return Error(400, "Parameter 'limit' must be a positive integer.");
        }
    }
    if (const auto parameter = request.Parameters.find("since"); parameter != request.Parameters.end())
    {
        std::uint64_t cursor = 0;
        if (!ReadNumber(parameter->second, cursor))
        {
            return Error(400, "Parameter 'since' must be a nonnegative integer.");
        }
        since = cursor;
    }

    const auto messages = Pine::Log::GetLogSnapshot();
    const auto latest = messages.empty() ? 0 : messages.back().Sequence;
    const auto oldest = messages.empty() ? 0 : messages.front().Sequence;
    if (since && *since > latest)
    {
        return Error(409, "Log cursor is ahead of this server's history.");
    }

    // Without a cursor preserve the existing most-recent-N behavior. With a cursor,
    // page forwards so a small limit never silently skips unread entries.
    auto entries = nlohmann::json::array();
    std::uint64_t next = since.value_or(latest);
    const auto offset = !since && limit < messages.size() ? messages.size() - limit : 0;
    for (std::size_t index = offset; index < messages.size(); ++index)
    {
        const auto& message = messages[index];
        if (since && message.Sequence <= *since)
        {
            continue;
        }
        if (entries.size() >= limit)
        {
            break;
        }
        entries.push_back({
            { "sequence", message.Sequence }, { "severity", SeverityName(message.Type) },
            { "message", message.Message }, { "file", message.FileName }, { "line", message.FileLine }
        });
        next = message.Sequence;
    }

    return { 200, {
        { "totalBuffered", messages.size() }, { "messages", entries },
        { "oldestSequence", oldest }, { "latestSequence", latest }, { "nextCursor", next },
        { "historyLost", since && oldest > 0 && *since < oldest - 1 },
        { "hasMore", next < latest }
    } };
}
