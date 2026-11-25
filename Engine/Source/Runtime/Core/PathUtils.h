#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace FPathUtils
{
    inline std::filesystem::path FromUTF8(std::string_view UTF8Text)
    {
        std::u8string UTF8Path;
        UTF8Path.reserve(UTF8Text.size());

        for (const unsigned char Character : UTF8Text)
        {
            UTF8Path.push_back(static_cast<char8_t>(Character));
        }

        return std::filesystem::path(UTF8Path);
    }
}
