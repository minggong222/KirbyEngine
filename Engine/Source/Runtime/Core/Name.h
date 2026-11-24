
#pragma once

#include "Core/STL.h"

using FNameEntryId = uint32;

enum EFindName : uint8
{
    FNAME_Find,
    FNAME_Add,
    FNAME_Replace
};

class FName
{
public:
    static constexpr int32 NAME_NO_NUMBER_INTERNAL = 0;

    constexpr FName() = default;

    FName(const char* InName);
    FName(const char* InName, EFindName FindType);
    FName(const char* InName, int32 InInternalNumber);

    FName(const FString& InName, EFindName FindType = FNAME_Add);
    FName(const FString& InName, int32 InInternalNumber);

    FName(FName Other, int32 InInternalNumber);

    bool IsNone() const;
    bool IsValid() const { return !IsNone(); }

    FString ToString() const;
    FString GetPlainNameString() const;

    uint32 GetComparisonIndex() const { return ComparisonIndex; }
    uint32 GetDisplayIndex() const { return DisplayIndex; }

    int32 GetNumber() const { return static_cast<int32>(Number); }
    int32 GetInternalNumber() const { return static_cast<int32>(Number); }

    void SetNumber(int32 NewInternalNumber);

    int32 Compare(const FName& Other) const;
    bool LexicalLess(const FName& Other) const;

    bool operator==(const FName& Other) const
    {
        return ComparisonIndex == Other.ComparisonIndex && Number == Other.Number;
    }

    bool operator!=(const FName& Other) const
    {
        return !(*this == Other);
    }

private:
    FNameEntryId ComparisonIndex = 0;
    FNameEntryId DisplayIndex = 0;
    uint32 Number = NAME_NO_NUMBER_INTERNAL;
};

inline const FName NAME_None{};

static_assert(sizeof(FName) == sizeof(uint32) * 3);
