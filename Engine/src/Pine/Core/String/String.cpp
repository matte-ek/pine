#include "String.hpp"
#include <algorithm>

bool Pine::String::StartsWith(const std::string& str, const std::string& start)
{
    return str.rfind(start, 0) == 0;
}

bool Pine::String::EndsWith(const std::string& str, const std::string& end)
{
    if (end.size() > str.size())
        return false;

    return std::equal(end.rbegin(), end.rend(), str.rbegin());
}

std::vector<std::string> Pine::String::Split(const std::string& str, const std::string& deli)
{
    // https://stackoverflow.com/a/37454181

    std::vector<std::string> tokens;
    std::size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(deli, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos - prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + deli.length();
    } while (pos < str.length() && prev < str.length());
    return tokens;
}

std::string Pine::String::Replace(const std::string& str, const std::string& pattern, const std::string& replacement)
{
    std::string s = str;

    if (!pattern.empty())
        for(std::size_t pos = 0; (pos = s.find(pattern, pos)) != std::string::npos; pos += replacement.size())
            s.replace(pos, pattern.size(), replacement);

    return s;
}

std::string Pine::String::ToLower(const std::string& str)
{
    std::string ret = str;

    std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c) { return std::tolower(c); });

    return ret;
}

std::string Pine::String::ToUpper(const std::string& str)
{
    std::string ret = str;

    std::transform(ret.begin(), ret.end(), ret.begin(), [](unsigned char c) { return std::toupper(c); });

    return ret;
}
