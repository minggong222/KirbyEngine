// Package (.kasset) loading and saving for FAssetManager.

#include "Resources/AssetManager.h"

#include "Core/Logging/Log.h"
#include "Core/PathUtils.h"
#include "Serialization/Package.h"
#include "UObject/Class.h"

#include "Resources/Material.h"
#include "Resources/StaticMesh.h"
#include "Resources/Texture2D.h"

#include <system_error>
#include <unordered_map>
#include <unordered_set>

struct FAssetManager::FPackageLoadState
{
    std::unordered_map<std::string, std::filesystem::path> PackagesByAssetPath;
    std::unordered_set<std::string> InProgress;
    std::unordered_set<std::string> Failed;
    int32 LoadedCount = 0;
};

bool FAssetManager::IsPackageUpToDate(const FString& AssetPath)
{
    std::error_code Error;

    const std::filesystem::path PackagePath = PackageFile::GetPackagePath(AssetPath);
    const std::filesystem::path SourcePath = FPathUtils::FromUTF8(AssetPath.ToStd());

    // The source file stays the asset's master copy: no source, no asset.
    if (!std::filesystem::exists(PackagePath, Error) || !std::filesystem::exists(SourcePath, Error))
    {
        return false;
    }

    const auto PackageTime = std::filesystem::last_write_time(PackagePath, Error);

    if (Error)
    {
        return false;
    }

    const auto SourceTime = std::filesystem::last_write_time(SourcePath, Error);
    return !Error && PackageTime >= SourceTime;
}

int32 FAssetManager::LoadPackages(FRHI& RHI, const std::filesystem::path& InContentDirectory)
{
    // Classes register themselves on first StaticClass(); make sure FindClass knows them.
    UStaticMesh::StaticClass();
    UMaterial::StaticClass();
    UTexture2D::StaticClass();

    const std::filesystem::path ContentDirectory = InContentDirectory.empty() ? std::filesystem::current_path() / "Content" : InContentDirectory;

    std::error_code Error;

    if (!std::filesystem::is_directory(ContentDirectory, Error))
    {
        return 0;
    }

    FPackageLoadState State;
    TArray<std::filesystem::path> OrphanPackages;

    for (auto It = std::filesystem::recursive_directory_iterator(ContentDirectory, std::filesystem::directory_options::skip_permission_denied, Error);
         !Error && It != std::filesystem::recursive_directory_iterator(); It.increment(Error))
    {
        if (!It->is_regular_file(Error) || It->path().extension() != PackageFile::Extension)
        {
            continue;
        }

        FPackageSummary Summary;

        if (!PackageFile::ReadSummary(It->path(), Summary))
        {
            UE_LOG(Warning, "Skipping unreadable package: %s", PathToUTF8(It->path()).c_str());
            continue;
        }

        const std::filesystem::path SourcePath = FPathUtils::FromUTF8(Summary.AssetPath.ToStd());

        if (!std::filesystem::exists(SourcePath, Error))
        {
            OrphanPackages.Add(It->path());
            continue;
        }

        if (IsPackageUpToDate(Summary.AssetPath))
        {
            State.PackagesByAssetPath[Summary.AssetPath.ToStd()] = It->path();
        }
    }

    // Packages are derived files; once their source asset is gone they are just stale copies.
    for (const std::filesystem::path& OrphanPackage : OrphanPackages)
    {
        std::filesystem::remove(OrphanPackage, Error);
    }

    for (const auto& Pair : State.PackagesByAssetPath)
    {
        LoadPackageRecursive(Pair.first, RHI, State);
    }

    UE_LOG(Info, "[Packages] Loaded %d package(s), %d skipped", State.LoadedCount, static_cast<int32>(State.Failed.size()));
    return State.LoadedCount;
}

UAsset* FAssetManager::LoadPackageRecursive(const std::string& AssetPath, FRHI& RHI, FPackageLoadState& State)
{
    if (UAsset** Existing = LoadedAssets.Find(AssetPath))
    {
        return *Existing;
    }

    if (State.Failed.contains(AssetPath) || State.InProgress.contains(AssetPath))
    {
        return nullptr;
    }

    const auto PackageIt = State.PackagesByAssetPath.find(AssetPath);

    if (PackageIt == State.PackagesByAssetPath.end())
    {
        return nullptr;
    }

    State.InProgress.insert(AssetPath);

    auto Fail = [&State, &AssetPath]() -> UAsset*
    {
        State.InProgress.erase(AssetPath);
        State.Failed.insert(AssetPath);
        return nullptr;
    };

    FArchiveFileReader Reader(PackageIt->second);
    FPackageSummary Summary;
    Reader << Summary;

    if (Reader.IsError() || Summary.Magic != PackageFile::Magic || Summary.Version != PackageFile::Version)
    {
        return Fail();
    }

    // Dependencies first. If one cannot come from a package, leave this asset to its
    // importer too, so it is never created with missing references.
    for (const FString& Dependency : Summary.Dependencies)
    {
        if (LoadPackageRecursive(Dependency.ToStd(), RHI, State) == nullptr)
        {
            return Fail();
        }
    }

    const UClass* AssetClass = UClass::FindClass(Summary.ClassName);
    UObject* NewObjectInstance = AssetClass != nullptr ? NewObject(AssetClass) : nullptr;
    UAsset* Asset = Cast<UAsset>(NewObjectInstance);

    if (Asset == nullptr || !Asset->SupportsPackage())
    {
        delete NewObjectInstance;
        return Fail();
    }

    Asset->AssetPath = Summary.AssetPath;
    Asset->Serialize(Reader);

    if (Reader.IsError() || !Asset->PostLoad(*this, RHI))
    {
        UE_LOG(Warning, "Failed to load package: %s", AssetPath.c_str());
        delete Asset;
        return Fail();
    }

    LoadedAssets.Add(AssetPath, Asset);
    State.InProgress.erase(AssetPath);
    ++State.LoadedCount;
    return Asset;
}

bool FAssetManager::SavePackage(UAsset* Asset)
{
    return Asset != nullptr && PackageFile::Save(*Asset);
}

int32 FAssetManager::SaveOutdatedPackages()
{
    int32 SavedCount = 0;
    std::error_code Error;

    for (const auto& Pair : LoadedAssets)
    {
        UAsset* Asset = Pair.second;

        // Only Content assets with a source file; built-in assets (/Engine/...) are generated in code.
        if (Asset == nullptr || !Asset->SupportsPackage() || !IsValidContentPath(Pair.first))
        {
            continue;
        }

        if (!std::filesystem::exists(FPathUtils::FromUTF8(Pair.first), Error) || IsPackageUpToDate(Asset->GetAssetPath()))
        {
            continue;
        }

        if (PackageFile::Save(*Asset))
        {
            ++SavedCount;
        }
        else
        {
            UE_LOG(Warning, "Failed to save package: %s", Pair.first.c_str());
        }
    }

    if (SavedCount > 0)
    {
        UE_LOG(Info, "[Packages] Saved %d package(s)", SavedCount);
    }

    return SavedCount;
}
