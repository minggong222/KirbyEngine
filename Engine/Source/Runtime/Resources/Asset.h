#pragma once
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Resources/Asset.generated.h"

class FAssetManager;

UCLASS()
class UAsset : public UObject
{
    GENERATED_BODY()
public:
    const FString& GetAssetPath() const { return AssetPath; }
private:
    friend class FAssetManager;
    FString AssetPath;
};
