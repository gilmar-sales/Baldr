#pragma once

#include <iomanip>
#include <sstream>
#include <string>

inline std::string trim(const std::string& str)
{
    const size_t start = str.find_first_not_of(" \t\n\r\f\v");

    const size_t end = str.find_last_not_of(" \t\n\r\f\v");

    if (start == std::string::npos || end == std::string::npos)
    {
        return "";
    }

    return str.substr(start, end - start + 1);
}

inline std::optional<std::string> decode_path(const std::string& path)
{
    std::ostringstream decoded;
    for (size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] == '%')
        {
            if (i + 2 < path.size())
            {
                std::string hex = path.substr(i + 1, 2);
                char        decodedChar =
                    static_cast<char>(std::stoi(hex, nullptr, 16));

                if (decodedChar == '\0')
                {
                    return std::nullopt;
                }

                decoded << decodedChar;
                i += 2; // Skip the next two characters
            }
            else
            {
                return std::nullopt;
            }
        }
        else
        {
            decoded << path[i];
        }
    }

    return decoded.str();
}