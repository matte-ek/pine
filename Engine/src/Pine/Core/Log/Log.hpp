#pragma once
#include <string_view>
#include <cstdint>
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

        std::uint64_t Sequence = 0;
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

    // Verbose messages are dropped unless this is on, which it is not by default. They are chatty
    // enough - every asset load, every undo step - to push real warnings out of the message
    // history, so they are something to switch on while looking for them.
    void SetVerboseEnabled(bool enabled);
    bool IsVerboseEnabled();

    // A consistent copy of the message history. Returns a copy rather than a reference because
    // background tasks log too, and the deque is mutated under a lock this leaves held.
    std::deque<LogMessage> GetLogSnapshot();
}
