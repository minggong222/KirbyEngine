#pragma once

#include "JsonValue.h"

class FJsonObject
{
public:
    void SetField(const FString& FieldName, TSharedPtr<FJsonValue> Value);
    void SetField(FString&& FieldName, TSharedPtr<FJsonValue> Value);

    void SetNullField(const FString& FieldName);
    void SetBoolField(const FString& FieldName, bool Value);
    void SetNumberField(const FString& FieldName, double Value);
    void SetStringField(const FString& FieldName, FString Value);

    void SetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>& Value);
    void SetArrayField(const FString& FieldName, TArray<TSharedPtr<FJsonValue>>&& Value);

    void SetObjectField(const FString& FieldName, const FJsonObject& Value);
    void SetObjectField(const FString& FieldName, FJsonObject&& Value);

    TSharedPtr<FJsonValue> GetField(const FString& FieldName) const;
    TSharedPtr<FJsonValue> GetField(const FString& FieldName, EJson Type) const;

    bool TryGetField(const FString& FieldName, TSharedPtr<FJsonValue>& OutValue) const;

    bool HasField(const FString& FieldName) const;
    bool RemoveField(const FString& FieldName);

    bool TryGetBoolField(const FString& FieldName, bool& OutValue) const;
    bool TryGetNumberField(const FString& FieldName, double& OutValue) const;
    bool TryGetStringField(const FString& FieldName, FString& OutValue) const;

    bool TryGetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutValue) const;
    bool TryGetArrayField(const FString& FieldName, TArray<TSharedPtr<FJsonValue>>*& OutValue);

    bool TryGetObjectField(const FString& FieldName, const TSharedPtr<FJsonObject>*& OutValue) const;
    bool TryGetObjectField(const FString& FieldName, TSharedPtr<FJsonObject>*& OutValue);

    bool GetBoolField(const FString& FieldName) const;
    double GetNumberField(const FString& FieldName) const;
    FString GetStringField(const FString& FieldName) const;

    const TArray<TSharedPtr<FJsonValue>>& GetArrayField(const FString& FieldName) const;
    const TSharedPtr<FJsonObject>& GetObjectField(const FString& FieldName) const;

    const TMap<FString, TSharedPtr<FJsonValue>>& GetValues() const;
    int32 Num() const;
    bool IsEmpty() const;

private:
    TMap<FString, TSharedPtr<FJsonValue>> Values;
};