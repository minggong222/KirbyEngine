#include "JsonValue.h"
#include "JsonObject.h"

FJsonValue::FJsonValue(EJson InType)
    : Type(InType)
{
}

bool FJsonValue::TryGetBool(bool& OutBool) const
{
    return false;
}

bool FJsonValue::TryGetNumber(double& OutNumber) const
{
    return false;
}

bool FJsonValue::TryGetString(FString& OutString) const
{
    return false;
}

bool FJsonValue::TryGetArray(const TArray<TSharedPtr<FJsonValue>>*& OutArray) const
{
    OutArray = nullptr;
    return false;
}

bool FJsonValue::TryGetArray(TArray<TSharedPtr<FJsonValue>>*& OutArray)
{
    OutArray = nullptr;
    return false;
}

bool FJsonValue::TryGetObject(const TSharedPtr<FJsonObject>*& OutObject) const
{
    OutObject = nullptr;
    return false;
}

bool FJsonValue::TryGetObject(TSharedPtr<FJsonObject>*& OutObject)
{
    OutObject = nullptr;
    return false;
}

bool FJsonValue::AsBool() const
{
    return false;
}

double FJsonValue::AsNumber() const
{
    return 0.0;
}

FString FJsonValue::AsString() const
{
    return FString();
}

const TArray<TSharedPtr<FJsonValue>>& FJsonValue::AsArray() const
{
    static const TArray<TSharedPtr<FJsonValue>> EmptyArray;
    return EmptyArray;
}

const TSharedPtr<FJsonObject>& FJsonValue::AsObject() const
{
    static const TSharedPtr<FJsonObject> EmptyObject;
    return EmptyObject;
}

bool FJsonValue::IsNull() const
{
    return Type == EJson::Null;
}

FJsonValueNull::FJsonValueNull()
    : FJsonValue(EJson::Null)
{
}

FJsonValueBoolean::FJsonValueBoolean(bool InValue)
    : FJsonValue(EJson::Boolean), Value(InValue)
{
}

bool FJsonValueBoolean::AsBool() const
{
    return Value;
}

bool FJsonValueBoolean::TryGetBool(bool& OutBool) const
{
    OutBool = Value;
    return true;
}

FJsonValueNumber::FJsonValueNumber(double InValue)
    : FJsonValue(EJson::Number), Value(InValue)
{
}

double FJsonValueNumber::AsNumber() const
{
    return Value;
}

bool FJsonValueNumber::TryGetNumber(double& OutNumber) const
{
    OutNumber = Value;
    return true;
}

FJsonValueString::FJsonValueString(FString InValue)
    : FJsonValue(EJson::String), Value(std::move(InValue))
{
}

FString FJsonValueString::AsString() const
{
    return Value;
}

bool FJsonValueString::TryGetString(FString& OutString) const
{
    OutString = Value;
    return true;
}

FJsonValueArray::FJsonValueArray()
    : FJsonValue(EJson::Array)
{
}

FJsonValueArray::FJsonValueArray(const TArray<TSharedPtr<FJsonValue>>& InArray)
    : FJsonValue(EJson::Array), Values(InArray)
{
}

FJsonValueArray::FJsonValueArray(TArray<TSharedPtr<FJsonValue>>&& InArray)
    : FJsonValue(EJson::Array), Values(std::move(InArray))
{
}

const TArray<TSharedPtr<FJsonValue>>& FJsonValueArray::AsArray() const
{
    return Values;
}

bool FJsonValueArray::TryGetArray(const TArray<TSharedPtr<FJsonValue>>*& OutArray) const
{
    OutArray = &Values;
    return true;
}

bool FJsonValueArray::TryGetArray(TArray<TSharedPtr<FJsonValue>>*& OutArray)
{
    OutArray = &Values;
    return true;
}

void FJsonValueArray::Add(const TSharedPtr<FJsonValue>& Value)
{
    if (Value.IsValid())
    {
        Values.Add(Value);
    }
}

void FJsonValueArray::Add(TSharedPtr<FJsonValue>&& Value)
{
    if (Value.IsValid())
    {
        Values.Add(std::move(Value));
    }
}

FJsonValueObject::FJsonValueObject(TSharedPtr<FJsonObject> InObject)
    : FJsonValue(EJson::Object), Value(InObject.IsValid() ? std::move(InObject) : MakeShared<FJsonObject>())
{
}

const TSharedPtr<FJsonObject>& FJsonValueObject::AsObject() const
{
    return Value;
}

bool FJsonValueObject::TryGetObject(const TSharedPtr<FJsonObject>*& OutObject) const
{
    OutObject = &Value;
    return true;
}

bool FJsonValueObject::TryGetObject(TSharedPtr<FJsonObject>*& OutObject)
{
    OutObject = &Value;
    return true;
}