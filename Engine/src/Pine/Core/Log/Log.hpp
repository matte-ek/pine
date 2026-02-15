#pragma once
#include <string_view>
#include <deque>
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
        const char* FileName;

        int FileLine;

        std::string Message;

        LogSeverity Type;
    };

}

namespace Pine::Log
{

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

#define Verbose(str) LogVerbose(__FILENAME__, __LINE__, str)
#define Info(str) LogInfo(__FILENAME__, __LINE__, str)
#define Warning(str) LogWarning(__FILENAME__, __LINE__, str)
#define Error(str) LogError(__FILENAME__, __LINE__, str)
#define Fatal(str) LogFatal(__FILENAME__, __LINE__, str)

    void LogVerbose(const char* fileName, int fileLine, std::string_view str);
    void LogInfo(const char* fileName, int fileLine, std::string_view str);
    void LogWarning(const char* fileName, int fileLine, std::string_view str);
    void LogError(const char* fileName, int fileLine, std::string_view str);
    void LogFatal(const char* fileName, int fileLine, std::string_view str);

    const std::deque<LogMessage>& GetLogMessages();
}