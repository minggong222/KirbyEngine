#pragma once

#include "Core/STL.h"

class FName
{
public:
    FName() = default;
    explicit FName(const char* InName);
    FName(const FString& InName, uint32 InNumber = 0);
    
    bool IsNone() const;
    FString ToString() const;
    
    uint32 GetComparisonIndex() const { return ComparisonIndex; }
    uint32 GetNumber() const { return Number; }
    
    bool operator==(const FName& Other) const
    {
        return ComparisonIndex == Other.ComparisonIndex && Number == Other.Number;
    }
    
    bool operator!=(const FName& Other) const
    {
        return !(*this == Other);
    }
private:
    static uint32 Intern(const FString& InName);
    static FString Resolve(uint32 InIndex);
    
    uint32 ComparisonIndex = 0;
    uint32 Number = 0;
};
