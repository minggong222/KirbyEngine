#pragma once

#include "Json/Dom/JsonValue.h"

class FJsonReader
{
public:
    explicit FJsonReader(const FString& InJsonString);

    bool Read(TSharedPtr<FJsonValue>& OutValue, FString& OutError);

private:
    bool ParseValue(TSharedPtr<FJsonValue>& OutValue, uint32 Depth);
    bool ParseObject(TSharedPtr<FJsonValue>& OutValue, uint32 Depth);
    bool ParseArray(TSharedPtr<FJsonValue>& OutValue, uint32 Depth);
    bool ParseString(FString& OutValue);
    bool ParseNumber(double& OutValue);
    bool ParseLiteral(const char* Literal);

    bool ParseHex4(uint32& OutValue);
    bool AppendCodePoint(std::string& OutString, uint32 CodePoint);

    void SkipWhitespace();
    bool IsDigit(char Character) const;
    bool Fail(const char* Message);

    std::string Source;
    std::string ErrorMessage;
    SIZE_T Cursor = 0;

    static constexpr uint32 MaxDepth = 128;
    static constexpr SIZE_T MaxInputBytes = 64 * 1024 * 1024;
};