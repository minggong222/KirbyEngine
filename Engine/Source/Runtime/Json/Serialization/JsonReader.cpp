#include "JsonReader.h"

#include "Json/Dom/JsonObject.h"
#include "JsonUTF8.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <cstring>

FJsonReader::FJsonReader(const FString& InJsonString)
    : Source(InJsonString.ToStd())
{
}

bool FJsonReader::Read(TSharedPtr<FJsonValue>& OutValue, FString& OutError)
{
    OutValue.Reset();
    ErrorMessage.clear();
    Cursor = 0;

    if (Source.size() > MaxInputBytes)
    {
        Fail("JSON input exceeds maximum size");
        OutError = FString(ErrorMessage);
        return false;
    }
    
    if (!JsonUTF8::IsValid(Source))
    {
        Fail("JSON input is not valid UTF-8");
        OutError = FString(ErrorMessage);
        return false;
    }

    SkipWhitespace();

    if (!ParseValue(OutValue, 0))
    {
        OutError = FString(ErrorMessage);
        return false;
    }

    SkipWhitespace();

    if (Cursor != Source.size())
    {
        Fail("Unexpected characters after JSON root value");
        OutValue.Reset();
        OutError = FString(ErrorMessage);
        return false;
    }

    OutError.Empty();
    return true;
}

bool FJsonReader::ParseValue(TSharedPtr<FJsonValue>& OutValue, uint32 Depth)
{
    if (Depth > MaxDepth)
    {
        return Fail("JSON nesting depth exceeded");
    }

    SkipWhitespace();

    if (Cursor >= Source.size())
    {
        return Fail("Unexpected end of JSON input");
    }

    const char Character = Source[Cursor];

    if (Character == '{')
    {
        return ParseObject(OutValue, Depth);
    }

    if (Character == '[')
    {
        return ParseArray(OutValue, Depth);
    }

    if (Character == '"')
    {
        FString StringValue;

        if (!ParseString(StringValue))
        {
            return false;
        }

        OutValue = MakeShared<FJsonValueString>(std::move(StringValue));
        return true;
    }

    if (Character == 't')
    {
        if (!ParseLiteral("true"))
        {
            return false;
        }

        OutValue = MakeShared<FJsonValueBoolean>(true);
        return true;
    }

    if (Character == 'f')
    {
        if (!ParseLiteral("false"))
        {
            return false;
        }

        OutValue = MakeShared<FJsonValueBoolean>(false);
        return true;
    }

    if (Character == 'n')
    {
        if (!ParseLiteral("null"))
        {
            return false;
        }

        OutValue = MakeShared<FJsonValueNull>();
        return true;
    }

    if (Character == '-' || IsDigit(Character))
    {
        double Number = 0.0;

        if (!ParseNumber(Number))
        {
            return false;
        }

        OutValue = MakeShared<FJsonValueNumber>(Number);
        return true;
    }

    return Fail("Invalid JSON value");
}

bool FJsonReader::ParseObject(TSharedPtr<FJsonValue>& OutValue, uint32 Depth)
{
    ++Cursor;

    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();

    SkipWhitespace();

    if (Cursor < Source.size() && Source[Cursor] == '}')
    {
        ++Cursor;
        OutValue = MakeShared<FJsonValueObject>(std::move(Object));
        return true;
    }

    while (Cursor < Source.size())
    {
        SkipWhitespace();

        FString Key;

        if (!ParseString(Key))
        {
            return Fail("Expected JSON object key");
        }

        if (Object->HasField(Key))
        {
            return Fail("Duplicate JSON object key");
        }

        SkipWhitespace();

        if (Cursor >= Source.size() || Source[Cursor] != ':')
        {
            return Fail("Expected ':' after JSON object key");
        }

        ++Cursor;

        TSharedPtr<FJsonValue> Value;

        if (!ParseValue(Value, Depth + 1))
        {
            return false;
        }

        Object->SetField(std::move(Key), std::move(Value));

        SkipWhitespace();

        if (Cursor >= Source.size())
        {
            return Fail("Unexpected end of JSON object");
        }

        if (Source[Cursor] == '}')
        {
            ++Cursor;
            OutValue = MakeShared<FJsonValueObject>(std::move(Object));
            return true;
        }

        if (Source[Cursor] != ',')
        {
            return Fail("Expected ',' or '}' in JSON object");
        }

        ++Cursor;
    }

    return Fail("Unterminated JSON object");
}

bool FJsonReader::ParseArray(TSharedPtr<FJsonValue>& OutValue, uint32 Depth)
{
    ++Cursor;

    TArray<TSharedPtr<FJsonValue>> Values;

    SkipWhitespace();

    if (Cursor < Source.size() && Source[Cursor] == ']')
    {
        ++Cursor;
        OutValue = MakeShared<FJsonValueArray>(std::move(Values));
        return true;
    }

    while (Cursor < Source.size())
    {
        TSharedPtr<FJsonValue> Value;

        if (!ParseValue(Value, Depth + 1))
        {
            return false;
        }

        Values.Add(std::move(Value));

        SkipWhitespace();

        if (Cursor >= Source.size())
        {
            return Fail("Unexpected end of JSON array");
        }

        if (Source[Cursor] == ']')
        {
            ++Cursor;
            OutValue = MakeShared<FJsonValueArray>(std::move(Values));
            return true;
        }

        if (Source[Cursor] != ',')
        {
            return Fail("Expected ',' or ']' in JSON array");
        }

        ++Cursor;
    }

    return Fail("Unterminated JSON array");
}

bool FJsonReader::ParseString(FString& OutValue)
{
    if (Cursor >= Source.size() || Source[Cursor] != '"')
    {
        return Fail("Expected JSON string");
    }

    ++Cursor;

    std::string Result;

    while (Cursor < Source.size())
    {
        const unsigned char Character = static_cast<unsigned char>(Source[Cursor++]);

        if (Character == '"')
        {
            OutValue = FString(std::move(Result));
            return true;
        }

        if (Character < 0x20)
        {
            return Fail("Unescaped control character in JSON string");
        }

        if (Character != '\\')
        {
            Result.push_back(static_cast<char>(Character));
            continue;
        }

        if (Cursor >= Source.size())
        {
            return Fail("Incomplete JSON escape sequence");
        }

        const char Escape = Source[Cursor++];

        switch (Escape)
        {
        case '"': Result.push_back('"'); break;
        case '\\': Result.push_back('\\'); break;
        case '/': Result.push_back('/'); break;
        case 'b': Result.push_back('\b'); break;
        case 'f': Result.push_back('\f'); break;
        case 'n': Result.push_back('\n'); break;
        case 'r': Result.push_back('\r'); break;
        case 't': Result.push_back('\t'); break;

        case 'u':
        {
            uint32 CodePoint = 0;

            if (!ParseHex4(CodePoint))
            {
                return false;
            }

            if (CodePoint >= 0xD800 && CodePoint <= 0xDBFF)
            {
                if (Cursor + 2 > Source.size() || Source[Cursor] != '\\' || Source[Cursor + 1] != 'u')
                {
                    return Fail("Missing low surrogate in JSON string");
                }

                Cursor += 2;

                uint32 LowSurrogate = 0;

                if (!ParseHex4(LowSurrogate))
                {
                    return false;
                }

                if (LowSurrogate < 0xDC00 || LowSurrogate > 0xDFFF)
                {
                    return Fail("Invalid low surrogate in JSON string");
                }

                CodePoint = 0x10000 + ((CodePoint - 0xD800) << 10) + (LowSurrogate - 0xDC00);
            }
            else if (CodePoint >= 0xDC00 && CodePoint <= 0xDFFF)
            {
                return Fail("Unexpected low surrogate in JSON string");
            }

            if (!AppendCodePoint(Result, CodePoint))
            {
                return false;
            }

            break;
        }

        default:
            return Fail("Invalid JSON escape sequence");
        }
    }

    return Fail("Unterminated JSON string");
}

bool FJsonReader::ParseNumber(double& OutValue)
{
    const SIZE_T Start = Cursor;

    if (Cursor < Source.size() && Source[Cursor] == '-')
    {
        ++Cursor;
    }

    if (Cursor >= Source.size())
    {
        return Fail("Incomplete JSON number");
    }

    if (Source[Cursor] == '0')
    {
        ++Cursor;

        if (Cursor < Source.size() && IsDigit(Source[Cursor]))
        {
            return Fail("Leading zero is not allowed in JSON number");
        }
    }
    else if (Source[Cursor] >= '1' && Source[Cursor] <= '9')
    {
        while (Cursor < Source.size() && IsDigit(Source[Cursor]))
        {
            ++Cursor;
        }
    }
    else
    {
        return Fail("Invalid JSON integer part");
    }

    if (Cursor < Source.size() && Source[Cursor] == '.')
    {
        ++Cursor;

        const SIZE_T FractionStart = Cursor;

        while (Cursor < Source.size() && IsDigit(Source[Cursor]))
        {
            ++Cursor;
        }

        if (Cursor == FractionStart)
        {
            return Fail("Missing digits after decimal point");
        }
    }

    if (Cursor < Source.size() && (Source[Cursor] == 'e' || Source[Cursor] == 'E'))
    {
        ++Cursor;

        if (Cursor < Source.size() && (Source[Cursor] == '+' || Source[Cursor] == '-'))
        {
            ++Cursor;
        }

        const SIZE_T ExponentStart = Cursor;

        while (Cursor < Source.size() && IsDigit(Source[Cursor]))
        {
            ++Cursor;
        }

        if (Cursor == ExponentStart)
        {
            return Fail("Missing exponent digits");
        }
    }

    const char* Begin = Source.data() + Start;
    const char* End = Source.data() + Cursor;

    const auto Result = std::from_chars(Begin, End, OutValue, std::chars_format::general);

    if (Result.ec != std::errc{} || Result.ptr != End || !std::isfinite(OutValue))
    {
        return Fail("JSON number is out of range");
    }

    return true;
}

bool FJsonReader::ParseLiteral(const char* Literal)
{
    const SIZE_T Length = std::strlen(Literal);

    if (Cursor + Length > Source.size() || Source.compare(Cursor, Length, Literal) != 0)
    {
        return Fail("Invalid JSON literal");
    }

    Cursor += Length;
    return true;
}

bool FJsonReader::ParseHex4(uint32& OutValue)
{
    OutValue = 0;

    if (Cursor + 4 > Source.size())
    {
        return Fail("Incomplete Unicode escape");
    }

    for (int32 Index = 0; Index < 4; ++Index)
    {
        const char Character = Source[Cursor++];
        uint32 Digit = 0;

        if (Character >= '0' && Character <= '9')
        {
            Digit = static_cast<uint32>(Character - '0');
        }
        else if (Character >= 'a' && Character <= 'f')
        {
            Digit = static_cast<uint32>(Character - 'a' + 10);
        }
        else if (Character >= 'A' && Character <= 'F')
        {
            Digit = static_cast<uint32>(Character - 'A' + 10);
        }
        else
        {
            return Fail("Invalid hexadecimal Unicode escape");
        }

        OutValue = (OutValue << 4) | Digit;
    }

    return true;
}

bool FJsonReader::AppendCodePoint(std::string& OutString, uint32 CodePoint)
{
    if (CodePoint <= 0x7F)
    {
        OutString.push_back(static_cast<char>(CodePoint));
    }
    else if (CodePoint <= 0x7FF)
    {
        OutString.push_back(static_cast<char>(0xC0 | (CodePoint >> 6)));
        OutString.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
    }
    else if (CodePoint <= 0xFFFF)
    {
        OutString.push_back(static_cast<char>(0xE0 | (CodePoint >> 12)));
        OutString.push_back(static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F)));
        OutString.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
    }
    else if (CodePoint <= 0x10FFFF)
    {
        OutString.push_back(static_cast<char>(0xF0 | (CodePoint >> 18)));
        OutString.push_back(static_cast<char>(0x80 | ((CodePoint >> 12) & 0x3F)));
        OutString.push_back(static_cast<char>(0x80 | ((CodePoint >> 6) & 0x3F)));
        OutString.push_back(static_cast<char>(0x80 | (CodePoint & 0x3F)));
    }
    else
    {
        return Fail("Invalid Unicode code point");
    }

    return true;
}

void FJsonReader::SkipWhitespace()
{
    while (Cursor < Source.size())
    {
        const char Character = Source[Cursor];

        if (Character == ' ' || Character == '\t' || Character == '\n' || Character == '\r')
        {
            ++Cursor;
            continue;
        }

        break;
    }
}

bool FJsonReader::IsDigit(char Character) const
{
    return Character >= '0' && Character <= '9';
}

bool FJsonReader::Fail(const char* Message)
{
    if (ErrorMessage.empty())
    {
        ErrorMessage = Message;
        ErrorMessage += " at byte ";
        ErrorMessage += std::to_string(Cursor);
    }

    return false;
}