#include "JsonObject.h"

void FJsonObject::SetField(const FString& FieldName, TSharedPtr<FJsonValue> Value)
{
    if (!Value.IsValid())
    {
        return;
    }

    Values.Add(FieldName, Value);
}

void FJsonObject::SetField(FString&& FieldName, TSharedPtr<FJsonValue> Value)
{
    if (!Value.IsValid())
    {
        return;
    }

    Values.Add(std::move(FieldName), std::move(Value));
}

void FJsonObject::SetNullField(const FString& FieldName)
{
    SetField(FieldName, MakeShared<FJsonValueNull>());
}

void FJsonObject::SetBoolField(const FString& FieldName, bool Value)
{
    SetField(FieldName, MakeShared<FJsonValueBoolean>(Value));
}

void FJsonObject::SetNumberField(const FString& FieldName, double Value)
{
    SetField(FieldName, MakeShared<FJsonValueNumber>(Value));
}

void FJsonObject::SetStringField(const FString& FieldName, FString Value)
{
    SetField(FieldName, MakeShared<FJsonValueString>(std::move(Value)));
}

void FJsonObject::SetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>& Value)
{
    SetField(FieldName, MakeShared<FJsonValueArray>(Value));
}

void FJsonObject::SetArrayField(const FString& FieldName, TArray<TSharedPtr<FJsonValue>>&& Value)
{
    SetField(FieldName, MakeShared<FJsonValueArray>(std::move(Value)));
}

void FJsonObject::SetObjectField(const FString& FieldName, const FJsonObject& Value)
{
    SetField(FieldName, MakeShared<FJsonValueObject>(MakeShared<FJsonObject>(Value)));
}

void FJsonObject::SetObjectField(const FString& FieldName, FJsonObject&& Value)
{
    SetField(FieldName, MakeShared<FJsonValueObject>(MakeShared<FJsonObject>(std::move(Value))));
}

TSharedPtr<FJsonValue> FJsonObject::GetField(const FString& FieldName) const
{
    const TSharedPtr<FJsonValue>* FoundValue = Values.Find(FieldName);
    return FoundValue != nullptr ? *FoundValue : TSharedPtr<FJsonValue>();
}

TSharedPtr<FJsonValue> FJsonObject::GetField(const FString& FieldName, EJson Type) const
{
    TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() && Value->Type == Type ? Value : TSharedPtr<FJsonValue>();
}

bool FJsonObject::TryGetField(const FString& FieldName, TSharedPtr<FJsonValue>& OutValue) const
{
    OutValue = GetField(FieldName);
    return OutValue.IsValid();
}

bool FJsonObject::HasField(const FString& FieldName) const
{
    return Values.Contains(FieldName);
}

bool FJsonObject::RemoveField(const FString& FieldName)
{
    return Values.Remove(FieldName) > 0;
}

bool FJsonObject::TryGetBoolField(const FString& FieldName, bool& OutValue) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() && Value->TryGetBool(OutValue);
}

bool FJsonObject::TryGetNumberField(const FString& FieldName, double& OutValue) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() && Value->TryGetNumber(OutValue);
}

bool FJsonObject::TryGetStringField(const FString& FieldName, FString& OutValue) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() && Value->TryGetString(OutValue);
}

bool FJsonObject::TryGetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutValue) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() && Value->TryGetArray(OutValue);
}

bool FJsonObject::TryGetArrayField(const FString& FieldName, TArray<TSharedPtr<FJsonValue>>*& OutValue)
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() && Value->TryGetArray(OutValue);
}

bool FJsonObject::TryGetObjectField(const FString& FieldName, const TSharedPtr<FJsonObject>*& OutValue) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() && Value->TryGetObject(OutValue);
}

bool FJsonObject::TryGetObjectField(const FString& FieldName, TSharedPtr<FJsonObject>*& OutValue)
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() && Value->TryGetObject(OutValue);
}

bool FJsonObject::GetBoolField(const FString& FieldName) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() ? Value->AsBool() : false;
}

double FJsonObject::GetNumberField(const FString& FieldName) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() ? Value->AsNumber() : 0.0;
}

FString FJsonObject::GetStringField(const FString& FieldName) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);
    return Value.IsValid() ? Value->AsString() : FString();
}

const TArray<TSharedPtr<FJsonValue>>& FJsonObject::GetArrayField(const FString& FieldName) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);

    if (Value.IsValid())
    {
        return Value->AsArray();
    }

    static const TArray<TSharedPtr<FJsonValue>> EmptyArray;
    return EmptyArray;
}

const TSharedPtr<FJsonObject>& FJsonObject::GetObjectField(const FString& FieldName) const
{
    const TSharedPtr<FJsonValue> Value = GetField(FieldName);

    if (Value.IsValid())
    {
        return Value->AsObject();
    }

    static const TSharedPtr<FJsonObject> EmptyObject;
    return EmptyObject;
}

const TMap<FString, TSharedPtr<FJsonValue>>& FJsonObject::GetValues() const
{
    return Values;
}

int32 FJsonObject::Num() const
{
    return Values.Num();
}

bool FJsonObject::IsEmpty() const
{
    return Values.IsEmpty();
}