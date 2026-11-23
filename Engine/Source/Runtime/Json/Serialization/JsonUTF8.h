#pragma once

#include <cstddef>
#include <string>

namespace JsonUTF8
{
    inline bool IsValid(const std::string& Text)
    {
        for (std::size_t Index = 0; Index < Text.size();)
        {
            const unsigned char Lead = static_cast<unsigned char>(Text[Index]);

            if (Lead <= 0x7F)
            {
                ++Index;
                continue;
            }

            std::size_t Length = 0;

            if (Lead >= 0xC2 && Lead <= 0xDF)
            {
                Length = 2;
            }
            else if (Lead >= 0xE0 && Lead <= 0xEF)
            {
                Length = 3;
            }
            else if (Lead >= 0xF0 && Lead <= 0xF4)
            {
                Length = 4;
            }
            else
            {
                return false;
            }

            if (Index + Length > Text.size())
            {
                return false;
            }

            const unsigned char Second = static_cast<unsigned char>(Text[Index + 1]);

            if ((Second & 0xC0) != 0x80)
            {
                return false;
            }

            if ((Lead == 0xE0 && Second < 0xA0) ||
                (Lead == 0xED && Second >= 0xA0) ||
                (Lead == 0xF0 && Second < 0x90) ||
                (Lead == 0xF4 && Second > 0x8F))
            {
                return false;
            }

            for (std::size_t Offset = 2; Offset < Length; ++Offset)
            {
                const unsigned char Continuation = static_cast<unsigned char>(Text[Index + Offset]);

                if ((Continuation & 0xC0) != 0x80)
                {
                    return false;
                }
            }

            Index += Length;
        }

        return true;
    }
}