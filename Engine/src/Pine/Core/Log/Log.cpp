#include "Log.hpp"

#include <deque>
#include <string>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <fmt/format.h>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
    std::deque<Pine::LogMessage> m_LogMessages;
    std::mutex m_LogMutex;

#ifdef _WIN32
    enum class ConsoleColor
    {
        None = 7,
        DarkGray = 8,
        Gray = 7,
        White = 15,
        Yellow = 14,
        Red = 12
    };

    void SetConsoleColor(ConsoleColor colorCode)
    {
        static auto consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleTextAttribute(consoleHandle,static_cast<int>(colorCode));
    }
#else
    enum class ConsoleColor
    {
        None,
        DarkGray = 90,
        Gray = 37,
        White = 97,
        Yellow = 33,
        Red = 31
    };

    void SetConsoleColor(ConsoleColor colorCode)
    {
        if (colorCode == ConsoleColor::None)
        {
            std::cout << "\033[;0m";
            return;
        }

        std::cout << "\033[;" << std::to_string(static_cast<int>(colorCode)) << "m";
    }
#endif

    void PrintMessage(
        const char* prefix,
        const char* fileName,
        int fileLine,
        ConsoleColor color,
        std::string_view str,
        ConsoleColor msgColor = ConsoleColor::None)
    {
        SetConsoleColor(color);

        std::cout << std::setw(10) << std::left << fmt::format("[{}] ", prefix);

        SetConsoleColor(ConsoleColor::DarkGray);

        std::cout << fmt::format("{}:{} ", fileName, fileLine);

        SetConsoleColor(msgColor);

        std::cout << str << std::endl;
    }

    void AddLogMessage(const Pine::LogMessage& message)
    {
        m_LogMessages.push_back(message);

        if (m_LogMessages.size() > 256)
        {
            m_LogMessages.erase(m_LogMessages.begin());
        }
    }
}

void Pine::Log::LogVerbose(const char* fileName, int fileLine, std::string_view str)
{
    return;

    std::unique_lock lck(m_LogMutex);

    PrintMessage("verbose", fileName, fileLine, ConsoleColor::DarkGray, str);
    AddLogMessage({fileName, fileLine, std::string(str), Pine::LogSeverity::Verbose});
}

void Pine::Log::LogInfo(const char* fileName, int fileLine, std::string_view str)
{
    std::unique_lock lck(m_LogMutex);

    PrintMessage("info", fileName, fileLine, ConsoleColor::White, str);
    AddLogMessage({fileName, fileLine, std::string(str), Pine::LogSeverity::Info});
}

void Pine::Log::LogWarning(const char* fileName, int fileLine, std::string_view str)
{
    std::unique_lock lck(m_LogMutex);

    PrintMessage("warning", fileName, fileLine,  ConsoleColor::Yellow, str);
    AddLogMessage({fileName, fileLine, std::string(str), Pine::LogSeverity::Warning});
}

void Pine::Log::LogError(const char* fileName, int fileLine, std::string_view str)
{
    std::unique_lock lck(m_LogMutex);

    PrintMessage("error", fileName, fileLine, ConsoleColor::Red, str);
    AddLogMessage({fileName, fileLine, std::string(str), Pine::LogSeverity::Error});
}

void Pine::Log::LogFatal(const char* fileName, int fileLine, std::string_view str)
{
    std::unique_lock lck(m_LogMutex);

    PrintMessage("fatal", fileName, fileLine, ConsoleColor::Red, str, ConsoleColor::Red);
    AddLogMessage({fileName, fileLine, std::string(str), Pine::LogSeverity::Fatal});
}

const std::deque<Pine::LogMessage> &Pine::Log::GetLogMessages()
{
    return m_LogMessages;
}
