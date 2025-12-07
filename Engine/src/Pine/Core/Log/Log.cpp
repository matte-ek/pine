#include "Log.hpp"

#include <iostream>
#include <mutex>
#include <fmt/format.h>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
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

    void PrintMessage(const char* prefix, ConsoleColor color, const char* str, ConsoleColor msgColor = ConsoleColor::None)
    {
        SetConsoleColor(color);

        std::cout << prefix << ": ";

        SetConsoleColor(msgColor);

        std::cout << str << std::endl;
    }

    std::vector<Pine::LogMessage> m_LogMessages;

    void AddLogMessage(const Pine::LogMessage& message)
    {
        m_LogMessages.push_back(message);

        if (m_LogMessages.size() > 256)
        {
            m_LogMessages.erase(m_LogMessages.begin());
        }
    }
}

void Pine::Log::Verbose(const std::string &str)
{
    std::unique_lock lck(m_LogMutex);

    PrintMessage("verbose", ConsoleColor::DarkGray, str.c_str(), ConsoleColor::DarkGray);
    AddLogMessage({str, Pine::LogSeverity::Verbose});
}

void Pine::Log::Info(const std::string &str)
{
    std::unique_lock lck(m_LogMutex);

    PrintMessage("info", ConsoleColor::White, str.c_str());
    AddLogMessage({str, Pine::LogSeverity::Info});
}

void Pine::Log::Warning(const std::string &str)
{
    std::unique_lock lck(m_LogMutex);

    PrintMessage("warning", ConsoleColor::Yellow, str.c_str());
    AddLogMessage({str, Pine::LogSeverity::Warning});
}

void Pine::Log::Error(const std::string &str)
{
    std::unique_lock lck(m_LogMutex);

    PrintMessage("error", ConsoleColor::Red, str.c_str());
    AddLogMessage({str, Pine::LogSeverity::Error});
}

void Pine::Log::Fatal(const std::string &str)
{
    std::unique_lock lck(m_LogMutex);

    PrintMessage("fatal", ConsoleColor::Red, str.c_str(), ConsoleColor::Red);
    AddLogMessage({str, Pine::LogSeverity::Fatal});
}

const std::vector<Pine::LogMessage> &Pine::Log::GetLogMessages()
{
    return m_LogMessages;
}
