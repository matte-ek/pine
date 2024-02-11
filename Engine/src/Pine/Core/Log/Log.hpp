#pragma once
#include <string>
#include <vector>
#include <fmt/format.h>

namespace Pine
{

    enum class LogSeverity
    {
        Verbose,
        Info,
        Warning,
        Error,
        Fatal
    };

    struct LogMessage
    {
        std::string Message;
        LogSeverity Type;
    };

}

namespace Pine::Log
{

    void Verbose(const std::string& str);
    void Info(const std::string& str);
    void Warning(const std::string& str);
    void Error(const std::string& str);
    void Fatal(const std::string& str);

    const std::vector<LogMessage>& GetLogMessages();

}