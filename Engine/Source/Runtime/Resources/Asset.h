#pragma once

#include "Core/STL.h"
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

    virtual void GetDependencies(TArray<UAsset*>& OutDependencies) const
    {
        OutDependencies.Reset();
    }

private:
    friend class FAssetManager;

    FString AssetPath;
};
