#include "Core/Precompiled.h"
#include "Name.h"
#include <mutex>

namespace
{
    struct FNameTable
    {
        std::mutex Mutex;
        std::unordered_map<FString, uint32> NameToIndex;
        std::vector<FString> DisplayNames;
        
        FNameTable()
        {
            DisplayNames.emplace_back();
        }
    };
    
    FNameTable& GetNameTable()
    {
        static FNameTable Table;
        return Table;
    }
    
    FString MakeComparisonKey(const FString& InName)
    {
        const std::string& Source = InName.ToStd();
        std::string Result;
        Result.reserve(Source.size());

        for (unsigned char Character : Source)
        {
            Result.push_back(static_cast<char>(std::tolower(Character)));
        }

        return FString(std::move(Result));
    }
}

FName::FName(const char* InName)
    : FName(InName != nullptr ? FString(InName) : FString(), 0)
{
}

FName::FName(const FString& InName, uint32 InNumber)
    : ComparisonIndex(Intern(InName)), Number(InNumber)
{
}

bool FName::IsNone() const
{
    return ComparisonIndex == 0;
}

FString FName::ToString() const
{
    FString Result = Resolve(ComparisonIndex);

    if (Number > 0)
    {
        Result.Append("_");
        Result.Append(std::to_string(Number));
    }

    return Result;
}

uint32 FName::Intern(const FString& InName)
{
    if (InName.empty())
    {
        return 0;
    }
    
    FNameTable& Table = GetNameTable();
    const FString ComparisonKey = MakeComparisonKey(InName);
    
    std::lock_guard<std::mutex> Lock(Table.Mutex);
    
    const auto Found = Table.NameToIndex.find(ComparisonKey);
    
    if (Found != Table.NameToIndex.end())
    {
        return Found->second;
    }
    
    const uint32 NewIndex = static_cast<uint32>(Table.DisplayNames.size());
    
    Table.DisplayNames.push_back(InName);
    Table.NameToIndex.emplace(ComparisonKey, NewIndex);
    
    return NewIndex;
}

FString FName::Resolve(uint32 InIndex)
{
    FNameTable& Table = GetNameTable();
    
    std::lock_guard<std::mutex> Lock(Table.Mutex);
    
    if (InIndex >= Table.DisplayNames.size())
    {
        return FString();
    }
    
    return Table.DisplayNames[InIndex];
}
