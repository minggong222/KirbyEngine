
#include "Core/Name.h"

#include <Windows.h>
#include <charconv>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    struct FNameEntryIds
    {
        FNameEntryId ComparisonIndex = 0;
        FNameEntryId DisplayIndex = 0;
    };

    class FNamePool
    {
    public:
        FNamePool()
        {
            ComparisonEntries.emplace_back("none");
            DisplayEntries.emplace_back("None");
            DefaultDisplayIndices.emplace_back(0);

            ComparisonLookup.emplace("none", 0);
            DisplayLookup.emplace("None", 0);
        }

        FNameEntryIds FindOrAdd(const FString& InName, EFindName FindType)
        {
            const std::string ComparisonKey = MakeComparisonKey(InName).ToStd();
            const std::string DisplayKey = InName.ToStd();

            if (ComparisonKey.empty() || ComparisonKey == "none")
            {
                return {};
            }

            std::lock_guard<std::mutex> Lock(Mutex);

            auto ComparisonIt = ComparisonLookup.find(ComparisonKey);
            FNameEntryId ComparisonIndex = 0;

            if (ComparisonIt == ComparisonLookup.end())
            {
                if (FindType == FNAME_Find)
                {
                    return {};
                }

                ComparisonIndex = static_cast<FNameEntryId>(ComparisonEntries.size());

                ComparisonEntries.push_back(ComparisonKey);
                DefaultDisplayIndices.push_back(0);
                ComparisonLookup.emplace(ComparisonKey, ComparisonIndex);
            }
            else
            {
                ComparisonIndex = ComparisonIt->second;
            }

            auto DisplayIt = DisplayLookup.find(DisplayKey);

            if (DisplayIt != DisplayLookup.end())
            {
                const FNameEntryId DisplayIndex = DisplayIt->second;

                if (FindType == FNAME_Replace)
                {
                    DefaultDisplayIndices[ComparisonIndex] = DisplayIndex;
                }

                return { ComparisonIndex, DisplayIndex };
            }

            if (FindType == FNAME_Find)
            {
                return { ComparisonIndex, DefaultDisplayIndices[ComparisonIndex] };
            }

            const FNameEntryId DisplayIndex = static_cast<FNameEntryId>(DisplayEntries.size());

            DisplayEntries.push_back(DisplayKey);
            DisplayLookup.emplace(DisplayKey, DisplayIndex);

            if (DefaultDisplayIndices[ComparisonIndex] == 0 || FindType == FNAME_Replace)
            {
                DefaultDisplayIndices[ComparisonIndex] = DisplayIndex;
            }

            return { ComparisonIndex, DisplayIndex };
        }

        FString GetDisplayName(FNameEntryId InIndex) const
        {
            std::lock_guard<std::mutex> Lock(Mutex);

            if (InIndex >= DisplayEntries.size())
            {
                return FString("None");
            }

            return FString(DisplayEntries[InIndex]);
        }

        FString GetComparisonName(FNameEntryId InIndex) const
        {
            std::lock_guard<std::mutex> Lock(Mutex);

            if (InIndex >= ComparisonEntries.size())
            {
                return FString("none");
            }

            return FString(ComparisonEntries[InIndex]);
        }

    private:
        static FString MakeComparisonKey(const FString& InName)
        {
            const std::string& Source = InName.ToStd();

            if (Source.empty())
            {
                return FString();
            }

            const int32 WideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Source.data(), static_cast<int32>(Source.size()), nullptr, 0);

            if (WideLength <= 0)
            {
                return MakeAsciiLower(Source);
            }

            std::wstring WideName(static_cast<std::size_t>(WideLength), L'\0');

            if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Source.data(), static_cast<int32>(Source.size()), WideName.data(), WideLength) <= 0)
            {
                return MakeAsciiLower(Source);
            }

            const int32 LowerLength = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, WideName.data(), WideLength, nullptr, 0, nullptr, nullptr, 0);

            if (LowerLength <= 0)
            {
                return MakeAsciiLower(Source);
            }

            std::wstring LowerName(static_cast<std::size_t>(LowerLength), L'\0');

            if (LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, WideName.data(), WideLength, LowerName.data(), LowerLength, nullptr, nullptr, 0) <= 0)
            {
                return MakeAsciiLower(Source);
            }

            const int32 UTF8Length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, LowerName.data(), LowerLength, nullptr, 0, nullptr, nullptr);

            if (UTF8Length <= 0)
            {
                return MakeAsciiLower(Source);
            }

            std::string Result(static_cast<std::size_t>(UTF8Length), '\0');

            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, LowerName.data(), LowerLength, Result.data(), UTF8Length, nullptr, nullptr) <= 0)
            {
                return MakeAsciiLower(Source);
            }

            return FString(std::move(Result));
        }

        static FString MakeAsciiLower(const std::string& InName)
        {
            std::string Result = InName;

            for (char& Character : Result)
            {
                Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
            }

            return FString(std::move(Result));
        }

        mutable std::mutex Mutex;

        std::unordered_map<std::string, FNameEntryId> ComparisonLookup;
        std::unordered_map<std::string, FNameEntryId> DisplayLookup;

        std::vector<std::string> ComparisonEntries;
        std::vector<std::string> DisplayEntries;

        std::vector<FNameEntryId> DefaultDisplayIndices;
    };

    FNamePool& GetNamePool()
    {
        static FNamePool Pool;
        return Pool;
    }

    bool ParseNumberFromName(const FString& InName, FString& OutBaseName, int32& OutInternalNumber)
    {
        const std::string& Source = InName.ToStd();
        const std::size_t SeparatorIndex = Source.find_last_of('_');

        if (SeparatorIndex == std::string::npos || SeparatorIndex == 0 || SeparatorIndex + 1 >= Source.size())
        {
            return false;
        }

        const std::string_view Suffix(Source.data() + SeparatorIndex + 1, Source.size() - SeparatorIndex - 1);

        if (Suffix.size() > 1 && Suffix.front() == '0')
        {
            return false;
        }

        uint32 ParsedNumber = 0;
        const auto ParseResult = std::from_chars(Suffix.data(), Suffix.data() + Suffix.size(), ParsedNumber);

        if (ParseResult.ec != std::errc() || ParseResult.ptr != Suffix.data() + Suffix.size() || ParsedNumber >= static_cast<uint32>(std::numeric_limits<int32>::max()))
        {
            return false;
        }

        OutBaseName = FString(Source.substr(0, SeparatorIndex));
        OutInternalNumber = static_cast<int32>(ParsedNumber + 1);

        return true;
    }
}

FName::FName(const char* InName)
    : FName(FString(InName != nullptr ? InName : ""), FNAME_Add)
{
}

FName::FName(const char* InName, EFindName FindType)
    : FName(FString(InName != nullptr ? InName : ""), FindType)
{
}

FName::FName(const char* InName, int32 InInternalNumber)
    : FName(FString(InName != nullptr ? InName : ""), InInternalNumber)
{
}

FName::FName(const FString& InName, EFindName FindType)
{
    if (InName.IsEmpty())
    {
        return;
    }

    FString BaseName = InName;
    int32 ParsedInternalNumber = NAME_NO_NUMBER_INTERNAL;

    ParseNumberFromName(InName, BaseName, ParsedInternalNumber);

    const FNameEntryIds Ids = GetNamePool().FindOrAdd(BaseName, FindType);

    if (Ids.ComparisonIndex == 0 && GetNamePool().GetComparisonName(0) != FString("none"))
    {
        return;
    }

    const FString ComparisonName = GetNamePool().GetComparisonName(Ids.ComparisonIndex);

    if (Ids.ComparisonIndex == 0 && ComparisonName != FString("none"))
    {
        return;
    }

    ComparisonIndex = Ids.ComparisonIndex;
    DisplayIndex = Ids.DisplayIndex;
    Number = static_cast<uint32>(ParsedInternalNumber);
}

FName::FName(const FString& InName, int32 InInternalNumber)
{
    if (InName.IsEmpty())
    {
        return;
    }

    const FNameEntryIds Ids = GetNamePool().FindOrAdd(InName, FNAME_Add);

    ComparisonIndex = Ids.ComparisonIndex;
    DisplayIndex = Ids.DisplayIndex;
    Number = static_cast<uint32>(std::max(0, InInternalNumber));
}

FName::FName(FName Other, int32 InInternalNumber)
    : ComparisonIndex(Other.ComparisonIndex), DisplayIndex(Other.DisplayIndex), Number(static_cast<uint32>(std::max(0, InInternalNumber)))
{
}

bool FName::IsNone() const
{
    return ComparisonIndex == 0 && Number == NAME_NO_NUMBER_INTERNAL;
}

FString FName::GetPlainNameString() const
{
    return GetNamePool().GetDisplayName(DisplayIndex);
}

FString FName::ToString() const
{
    FString Result = GetPlainNameString();

    if (Number > NAME_NO_NUMBER_INTERNAL)
    {
        Result.Append("_");
        Result.Append(std::to_string(Number - 1));
    }

    return Result;
}

void FName::SetNumber(int32 NewInternalNumber)
{
    Number = static_cast<uint32>(std::max(0, NewInternalNumber));
}

int32 FName::Compare(const FName& Other) const
{
    if (ComparisonIndex < Other.ComparisonIndex)
    {
        return -1;
    }

    if (ComparisonIndex > Other.ComparisonIndex)
    {
        return 1;
    }

    if (Number < Other.Number)
    {
        return -1;
    }

    if (Number > Other.Number)
    {
        return 1;
    }

    return 0;
}

bool FName::LexicalLess(const FName& Other) const
{
    const FString ThisName = GetNamePool().GetComparisonName(ComparisonIndex);
    const FString OtherName = GetNamePool().GetComparisonName(Other.ComparisonIndex);

    if (ThisName != OtherName)
    {
        return ThisName < OtherName;
    }

    return Number < Other.Number;
}