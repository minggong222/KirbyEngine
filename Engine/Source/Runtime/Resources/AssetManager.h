#pragma once
#include "Asset.h"
#include "Core/STL.h"

#include <string>
#include <type_traits>
#include <utility>

class FAssetManager
{
public:
    FAssetManager() = default;
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
        if (FoundAsset == nullptr || *FoundAsset == nullptr)
        {
            return nullptr;
        }
        if (!(*FoundAsset)->IsA<TAsset>())
        {
            return nullptr;
        }

        return static_cast<TAsset*>(*FoundAsset);
    }
    
    void Shutdown();
    int32 GetLoadedAssetCount() const {return LoadedAssets.Num();}
private:
    TMap<std::string, UAsset*> LoadedAssets;    
};
