#include "JsonSerializer.h"

#include "JsonReader.h"
#include "JsonWriter.h"

bool FJsonSerializer::Serialize(const TSharedPtr<FJsonValue>& RootValue, FString& OutJson, bool bPrettyPrint, FString* OutError)
{
    FJsonWriter Writer(bPrettyPrint);

    FString Error;

    if (!Writer.Write(RootValue, OutJson, Error))
    {
        if (OutError != nullptr)
        {
            *OutError = Error;
        }

        return false;
    }

    if (OutError != nullptr)
    {
        OutError->Empty();
    }

    return true;
}

bool FJsonSerializer::Serialize(const TSharedPtr<FJsonObject>& RootObject, FString& OutJson, bool bPrettyPrint, FString* OutError)
{
    if (!RootObject.IsValid())
    {
        if (OutError != nullptr)
        {
            *OutError = "Invalid JSON root object";
        }

        return false;
    }

    TSharedPtr<FJsonValue> RootValue = MakeShared<FJsonValueObject>(RootObject);
    return Serialize(RootValue, OutJson, bPrettyPrint, OutError);
}

bool FJsonSerializer::Deserialize(const FString& JsonString, TSharedPtr<FJsonValue>& OutValue, FString* OutError)
{
    FJsonReader Reader(JsonString);

    FString Error;

    if (!Reader.Read(OutValue, Error))
    {
        if (OutError != nullptr)
        {
            *OutError = Error;
        }

        return false;
    }

    if (OutError != nullptr)
    {
        OutError->Empty();
    }

    return true;
}

bool FJsonSerializer::DeserializeObject(const FString& JsonString, TSharedPtr<FJsonObject>& OutObject, FString* OutError)
{
    OutObject.Reset();

    TSharedPtr<FJsonValue> RootValue;
    FString Error;

    if (!Deserialize(JsonString, RootValue, &Error))
    {
        if (OutError != nullptr)
        {
            *OutError = Error;
        }

        return false;
    }

    const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;

    if (!RootValue->TryGetObject(ObjectPtr) || ObjectPtr == nullptr || !ObjectPtr->IsValid())
    {
        if (OutError != nullptr)
        {
            *OutError = "JSON root value is not an object";
        }

        return false;
    }

    OutObject = *ObjectPtr;

    if (OutError != nullptr)
    {
        OutError->Empty();
    }

    return true;
}