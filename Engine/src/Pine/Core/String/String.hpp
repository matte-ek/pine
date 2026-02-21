#pragma once
#include <string>
#include <vector>

// Utility functions for strings
namespace Pine::String
{

    bool StartsWith(const std::string& str, const std::string& start);
    bool EndsWith(const std::string& str, const std::string& end);

    std::string ToLower(const std::string& str);
    std::string ToUpper(const std::string& str);

    std::string Trim(std::string str);

    std::vector<std::string> Split(const std::string& str, const std::string& deli);

    std::string Replace(const std::string& str, const std::string& pattern, const std::string& replacement);

}