#pragma once
#include <string>
#include <fmt/format.h>

namespace Pine::Log
{

    void Verbose(const std::string& str);
    void Message(const std::string& str);
    void Warning(const std::string& str);
    void Error(const std::string& str);
    void Fatal(const std::string& str);

}