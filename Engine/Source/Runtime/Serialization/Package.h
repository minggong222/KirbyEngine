#pragma once

#include "Core/STL.h"
#include "Serialization/Archive.h"

#include <filesystem>

class UAsset;

// Header at the start of every .kasset package file (Unreal's FPackageFileSummary).
//
//   .kasset = FPackageSummary + the asset's own data (UAsset::Serialize)
//
// Dependencies are the asset paths this asset references; they are loaded first.
struct FPackageSummary
{
    uint32 Magic = 0;
    uint32 Version = 0;
    FString ClassName;
    FString AssetPath;
    TArray<FString> Dependencies;
};

namespace PackageFile
{
    inline constexpr uint32 Magic = 0x5453414B; // "KAST"
    inline constexpr uint32 Version = 1;
    inline constexpr char Extension[] = ".kasset";

    // Package file for an asset path: "<asset path>.kasset", next to the source file.
    std::filesystem::path GetPackagePath(const FString& AssetPath);

    // Writes the summary and the asset's data. Returns false on any I/O error.
    bool Save(UAsset& Asset);

    // Reads only the summary (used to find dependencies before loading).
    bool ReadSummary(const std::filesystem::path& PackagePath, FPackageSummary& OutSummary);
}

FArchive& operator<<(FArchive& Ar, FPackageSummary& Summary);
