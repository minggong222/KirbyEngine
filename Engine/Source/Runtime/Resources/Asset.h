#pragma once
#include "UObject/Class.h"
#include "UObject/Object.h"

class FAssetManager;

class UAsset : public UObject
{
    DECLARE_CLASS(UAsset, UObject)
public:
    const FString& GetAssetPath() const { return AssetPath; }
private:
    friend class FAssetManager;
    FString AssetPath;
};
