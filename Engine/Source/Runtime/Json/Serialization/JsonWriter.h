#pragma once

#include "Json/Dom/JsonValue.h"

class FJsonWriter
{
public:
    explicit FJsonWriter(bool bInPrettyPrint = true, uint32 InIndentSize = 4);

    bool Write(const TSharedPtr<FJsonValue>& RootValue, FString& OutJson, FString& OutError);

private:
    bool WriteValue(const TSharedPtr<FJsonValue>& Value, uint32 Depth);
    bool WriteObject(const FJsonObject& Object, uint32 Depth);
    bool WriteArray(const TArray<TSharedPtr<FJsonValue>>& Values, uint32 Depth);

    bool WriteString(const FString& Value);
    void WriteIndent(uint32 Depth);
    bool WriteNumber(double Number);

    bool Fail(const char* Message);

    bool bPrettyPrint = true;
    uint32 IndentSize = 4;

    std::string Output;
    FString ErrorMessage;

    static constexpr uint32 MaxDepth = 128;
};