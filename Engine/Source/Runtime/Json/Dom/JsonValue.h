#pragma once

#include "Core/STL.h"

enum class EJson : uint8
{
    None,
    Null,
    String,
    Number,
    Boolean,
    Array,
    Object
};

class FJsonObject;

class FJsonValue
{
public:
    virtual ~FJsonValue() = default;

    EJson Type = EJson::None;

    virtual bool TryGetBool(bool& OutBool) const;
    virtual bool TryGetNumber(double& OutNumber) const;
    virtual bool TryGetString(FString& OutString) const;

    virtual bool TryGetArray(const TArray<TSharedPtr<FJsonValue>>*& OutArray) const;
    virtual bool TryGetArray(TArray<TSharedPtr<FJsonValue>>*& OutArray);

    virtual bool TryGetObject(const TSharedPtr<FJsonObject>*& OutObject) const;
    virtual bool TryGetObject(TSharedPtr<FJsonObject>*& OutObject);

    virtual bool AsBool() const;
    virtual double AsNumber() const;
    virtual FString AsString() const;

    virtual const TArray<TSharedPtr<FJsonValue>>& AsArray() const;
    virtual const TSharedPtr<FJsonObject>& AsObject() const;

    bool IsNull() const;

protected:
    explicit FJsonValue(EJson InType);
};

class FJsonValueNull : public FJsonValue
{
public:
    FJsonValueNull();
};

class FJsonValueBoolean : public FJsonValue
{
public:
    explicit FJsonValueBoolean(bool InValue);

    bool AsBool() const override;
    bool TryGetBool(bool& OutBool) const override;

private:
    bool Value = false;
};

class FJsonValueNumber : public FJsonValue
{
public:
    explicit FJsonValueNumber(double InValue);

    double AsNumber() const override;
    bool TryGetNumber(double& OutNumber) const override;

private:
    double Value = 0.0;
};

class FJsonValueString : public FJsonValue
{
public:
    explicit FJsonValueString(FString InValue);

    FString AsString() const override;
    bool TryGetString(FString& OutString) const override;

private:
    FString Value;
};

class FJsonValueArray : public FJsonValue
{
public:
    FJsonValueArray();
    explicit FJsonValueArray(const TArray<TSharedPtr<FJsonValue>>& InArray);
    explicit FJsonValueArray(TArray<TSharedPtr<FJsonValue>>&& InArray);

    const TArray<TSharedPtr<FJsonValue>>& AsArray() const override;

    bool TryGetArray(const TArray<TSharedPtr<FJsonValue>>*& OutArray) const override;
    bool TryGetArray(TArray<TSharedPtr<FJsonValue>>*& OutArray) override;

    void Add(const TSharedPtr<FJsonValue>& Value);
    void Add(TSharedPtr<FJsonValue>&& Value);

private:
    TArray<TSharedPtr<FJsonValue>> Values;
};

class FJsonValueObject : public FJsonValue
{
public:
    explicit FJsonValueObject(TSharedPtr<FJsonObject> InObject);

    const TSharedPtr<FJsonObject>& AsObject() const override;

    bool TryGetObject(const TSharedPtr<FJsonObject>*& OutObject) const override;
    bool TryGetObject(TSharedPtr<FJsonObject>*& OutObject) override;

private:
    TSharedPtr<FJsonObject> Value;
};