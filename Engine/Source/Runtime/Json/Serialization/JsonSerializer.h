#pragma once

#include "Json/Dom/JsonObject.h"

class FJsonSerializer
{
public:
    static bool Serialize(const TSharedPtr<FJsonValue>& RootValue, FString& OutJson, bool bPrettyPrint = true, FString* OutError = nullptr);
    static bool Serialize(const TSharedPtr<FJsonObject>& RootObject, FString& OutJson, bool bPrettyPrint = true, FString* OutError = nullptr);

    static bool Deserialize(const FString& JsonString, TSharedPtr<FJsonValue>& OutValue, FString* OutError = nullptr);
    static bool DeserializeObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutObject, FString* OutError = nullptr);
};