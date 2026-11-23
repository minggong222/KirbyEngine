#include "JsonWriter.h"

#include "Json/Dom/JsonObject.h"
#include "JsonUTF8.h"

#include <charconv>
#include <cmath>
#include <limits>

FJsonWriter::FJsonWriter(bool bInPrettyPrint, uint32 InIndentSize)
    : bPrettyPrint(bInPrettyPrint), IndentSize(InIndentSize)
{
}

bool FJsonWriter::Write(const TSharedPtr<FJsonValue>& RootValue, FString& OutJson, FString& OutError)
{
    Output.clear();
    ErrorMessage.Empty();
    OutJson.Empty();

    if (!RootValue.IsValid())
    {
        Fail("Cannot serialize invalid JSON value");
        OutError = ErrorMessage;
        return false;
    }

    if (!WriteValue(RootValue, 0))
    {
        OutError = ErrorMessage;
        return false;
    }

    OutJson = FString(std::move(Output));
    OutError.Empty();
    return true;
}

bool FJsonWriter::WriteValue(const TSharedPtr<FJsonValue>& Value, uint32 Depth)
{
    if (!Value.IsValid())
    {
        return Fail("Invalid JSON value");
    }

    if (Depth > MaxDepth)
    {
        return Fail("JSON nesting depth exceeded");
    }

    switch (Value->Type)
    {
    case EJson::Null:
        Output += "null";
        return true;

    case EJson::Boolean:
        Output += Value->AsBool() ? "true" : "false";
        return true;

    case EJson::Number:
        return WriteNumber(Value->AsNumber());

    case EJson::String:
        return WriteString(Value->AsString());

    case EJson::Array:
        return WriteArray(Value->AsArray(), Depth);

    case EJson::Object:
    {
        const TSharedPtr<FJsonObject>& Object = Value->AsObject();

        if (!Object.IsValid())
        {
            return Fail("Invalid JSON object value");
        }

        return WriteObject(*Object, Depth);
    }

    default:
        return Fail("Unsupported JSON value type");
    }
}

bool FJsonWriter::WriteObject(const FJsonObject& Object, uint32 Depth)
{
    Output.push_back('{');

    TArray<FString> Keys;
    Object.GetValues().GenerateKeyArray(Keys);
    Keys.Sort([](const FString& A, const FString& B) { return A < B; });

    int32 Index = 0;

    for (const FString& Key : Keys)
    {
        const TSharedPtr<FJsonValue> Value = Object.GetField(Key);

        if (!Value.IsValid())
        {
            return Fail("JSON object contains invalid field value");
        }

        if (Index > 0)
        {
            Output.push_back(',');
        }

        if (bPrettyPrint)
        {
            WriteIndent(Depth + 1);
        }

        if (!WriteString(Key))
        {
            return false;
        }

        Output += bPrettyPrint ? ": " : ":";

        if (!WriteValue(Value, Depth + 1))
        {
            return false;
        }

        ++Index;
    }

    if (Index > 0 && bPrettyPrint)
    {
        WriteIndent(Depth);
    }

    Output.push_back('}');
    return true;
}

bool FJsonWriter::WriteArray(const TArray<TSharedPtr<FJsonValue>>& Values, uint32 Depth)
{
    Output.push_back('[');

    for (int32 Index = 0; Index < Values.Num(); ++Index)
    {
        if (Index > 0)
        {
            Output.push_back(',');
        }

        if (bPrettyPrint)
        {
            WriteIndent(Depth + 1);
        }

        if (!WriteValue(Values[Index], Depth + 1))
        {
            return false;
        }
    }

    if (!Values.IsEmpty() && bPrettyPrint)
    {
        WriteIndent(Depth);
    }

    Output.push_back(']');
    return true;
}

bool FJsonWriter::WriteString(const FString& Value)
{
    static constexpr char HexDigits[] = "0123456789ABCDEF";

    const std::string& Text = Value.ToStd();

    if (!JsonUTF8::IsValid(Text))
    {
        return Fail("JSON string contains invalid UTF-8");
    }

    Output.push_back('"');

    for (unsigned char Character : Text)
    {
        switch (Character)
        {
        case '"': Output += "\\\""; break;
        case '\\': Output += "\\\\"; break;
        case '\b': Output += "\\b"; break;
        case '\f': Output += "\\f"; break;
        case '\n': Output += "\\n"; break;
        case '\r': Output += "\\r"; break;
        case '\t': Output += "\\t"; break;

        default:
            if (Character < 0x20)
            {
                Output += "\\u00";
                Output.push_back(HexDigits[(Character >> 4) & 0x0F]);
                Output.push_back(HexDigits[Character & 0x0F]);
            }
            else
            {
                Output.push_back(static_cast<char>(Character));
            }
            break;
        }
    }

    Output.push_back('"');
    return true;
}

void FJsonWriter::WriteIndent(uint32 Depth)
{
    Output.push_back('\n');
    Output.append(static_cast<SIZE_T>(Depth) * IndentSize, ' ');
}

bool FJsonWriter::WriteNumber(double Number)
{
    if (!std::isfinite(Number))
    {
        return Fail("JSON cannot represent NaN or Infinity");
    }

    char Buffer[64] = {};

    const auto Result = std::to_chars(Buffer, Buffer + sizeof(Buffer), Number, std::chars_format::general, std::numeric_limits<double>::max_digits10);

    if (Result.ec != std::errc{})
    {
        return Fail("Failed to format JSON number");
    }

    Output.append(Buffer, Result.ptr);
    return true;
}

bool FJsonWriter::Fail(const char* Message)
{
    if (ErrorMessage.IsEmpty())
    {
        ErrorMessage = FString(Message);
    }

    return false;
}