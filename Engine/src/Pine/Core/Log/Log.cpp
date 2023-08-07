#include "Log.hpp"

#include <iostream>
#include <fmt/format.h>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
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
        DarkGray = 30,
        Gray = 30,
        White = 37,
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

    void PrintMessage(const char* prefix, ConsoleColor color, const char* str)
    {
        SetConsoleColor(color);

        std::cout << "[" << prefix << "] ";

        SetConsoleColor(ConsoleColor::White);

        std::cout << str << std::endl;
    }
}

void Pine::Log::Verbose(const std::string &str)
{
    PrintMessage("Verbose", ConsoleColor::White, str.c_str());
}

void Pine::Log::Message(const std::string &str)
{
    PrintMessage("Message", ConsoleColor::White, str.c_str());
}

void Pine::Log::Warning(const std::string &str)
{
    PrintMessage("Warning", ConsoleColor::Yellow, str.c_str());
}

void Pine::Log::Error(const std::string &str)
{
    PrintMessage("Error", ConsoleColor::Red, str.c_str());
}

void Pine::Log::Fatal(const std::string &str)
{
    PrintMessage("FATAL", ConsoleColor::Red, str.c_str());
}
