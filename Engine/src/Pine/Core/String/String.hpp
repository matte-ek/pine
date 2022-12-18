#pragma once
#include <string>
#include <vector>

// Utility functions for strings
namespace Pine::String
{

    bool StartsWith(const std::string& str, const std::string& start);
    bool EndsWith(const std::string& str, const std::string& end);

    std::vector<std::string> Split(const std::string& str, const std::string& deli);

    std::string Replace(const std::string& str, const std::string& pattern, const std::string& replacement);

}