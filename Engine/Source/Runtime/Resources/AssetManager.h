#pragma once

#include "Asset.h"
#include "Core/STL.h"

#include <filesystem>
#include <string>
#include <type_traits>
#include <utility>

struct FAssetRegistryEntry
{
    std::string AssetPath;
    std::string AssetType;
    uint64 FileSize = 0;
};

class FAssetManager
{
public:
    FAssetManager();
    ~FAssetManager() { Shutdown(); }

    template<typename TAsset, typename TFactory>
    TAsset* Load(const FString& AssetPath, TFactory&& Factory)
    {
        static_assert(std::is_base_of_v<UAsset, TAsset>, "TAsset must derive from UAsset");

        if (AssetPath.IsEmpty())
        {
            return nullptr;
        }

        UAsset** ExistingAsset = LoadedAssets.Find(AssetPath.ToStd());

        if (ExistingAsset != nullptr)
        {
            if (*ExistingAsset == nullptr || !(*ExistingAsset)->IsA<TAsset>())
            {
                assert(false && "Asset type does not match cached asset");
                return nullptr;
            }

            return static_cast<TAsset*>(*ExistingAsset);
        }

        TAsset* NewAsset = std::forward<TFactory>(Factory)();

        if (NewAsset == nullptr)
        {
            return nullptr;
        }

        NewAsset->AssetPath = AssetPath;
        LoadedAssets.Add(AssetPath.ToStd(), NewAsset);
        return NewAsset;
    }

    template<typename TAsset>
    TAsset* Find(const FString& AssetPath)
    {
        static_assert(std::is_base_of_v<UAsset, TAsset>, "TAsset must derive from UAsset");

        UAsset** FoundAsset = LoadedAssets.Find(AssetPath.ToStd());

        if (FoundAsset == nullptr || *FoundAsset == nullptr || !(*FoundAsset)->IsA<TAsset>())
        {
            return nullptr;
        }

        return static_cast<TAsset*>(*FoundAsset);
    }

    template<typename TAsset>
    const TAsset* Find(const FString& AssetPath) const
    {
        static_assert(std::is_base_of_v<UAsset, TAsset>, "TAsset must derive from UAsset");

        UAsset* const* FoundAsset = LoadedAssets.Find(AssetPath.ToStd());

        if (FoundAsset == nullptr || *FoundAsset == nullptr || !(*FoundAsset)->IsA<TAsset>())
        {
            return nullptr;
        }

        return static_cast<const TAsset*>(*FoundAsset);
    }

    template<typename TAsset>
    void GetAssets(TArray<TAsset*>& OutAssets) const
    {
        static_assert(std::is_base_of_v<UAsset, TAsset>, "TAsset must derive from UAsset");
        OutAssets.Reset();

        for (const auto& Pair : LoadedAssets)
        {
            UAsset* Asset = Pair.second;

            if (Asset != nullptr && Asset->IsA<TAsset>())
            {
                OutAssets.Add(static_cast<TAsset*>(Asset));
            }
        }

        OutAssets.Sort([](const TAsset* A, const TAsset* B)
        {
            return A->GetAssetPath() < B->GetAssetPath();
        });
    }

    UAsset* FindAny(const FString& AssetPath) const;

    bool RepathAsset(UAsset* Asset, const std::string& NewAssetPath);
    bool RepathAssets(const std::string& OldPrefix, const std::string& NewPrefix);
    bool RemoveAsset(UAsset* Asset);

    void GetDependencies(UAsset* Asset, TArray<UAsset*>& OutDependencies) const;
    void GetReferencers(UAsset* Asset, TArray<UAsset*>& OutReferencers) const;

    void RefreshAssetRegistry(const std::filesystem::path& ContentDirectory = {});
    void GetAssetRegistryEntries(TArray<FAssetRegistryEntry>& OutEntries) const;
    bool FindAssetRegistryEntry(const std::string& AssetPath, FAssetRegistryEntry& OutEntry) const;

    int32 GetLoadedAssetCount() const { return LoadedAssets.Num(); }
    int32 GetRegisteredAssetCount() const { return AssetRegistry.Num(); }
    void Shutdown();

private:
    bool IsValidContentPath(const std::string& AssetPath) const;
    static std::string GetAssetTypeFromExtension(const std::string& Extension);
    static std::string PathToUTF8(const std::filesystem::path& Path);

    TMap<std::string, UAsset*> LoadedAssets;
    TMap<std::string, FAssetRegistryEntry> AssetRegistry;
};
