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

#ifdef _WIN32
#define PINE_FILE_NAME (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define PINE_FILE_NAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

#define PVerbose(str) Pine::Log::LogVerbose(PINE_FILE_NAME, __LINE__, str)
#define PInfo(str) Pine::Log::LogInfo(PINE_FILE_NAME, __LINE__, str)
#define PWarning(str) Pine::Log::LogWarning(PINE_FILE_NAME, __LINE__, str)
#define PError(str) Pine::Log::LogError(PINE_FILE_NAME, __LINE__, str)
#define PFatal(str) Pine::Log::LogFatal(PINE_FILE_NAME, __LINE__, str)

namespace Pine::Log
{
    void LogVerbose(const char* fileName, int fileLine, std::string_view str);
    void LogInfo(const char* fileName, int fileLine, std::string_view str);
    void LogWarning(const char* fileName, int fileLine, std::string_view str);
    void LogError(const char* fileName, int fileLine, std::string_view str);
    void LogFatal(const char* fileName, int fileLine, std::string_view str);

    const std::deque<LogMessage>& GetLogMessages();
}