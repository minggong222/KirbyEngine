#pragma once

#include "Core/STL.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Resources/Asset.generated.h"

class FAssetManager;
class FArchive;
class FRHI;

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

    // ---- Packages (.kasset) -------------------------------------------------
    // Asset types that can be stored in a package override all three.

    virtual bool SupportsPackage() const { return false; }

    // Reads or writes the asset's data. References to other assets are stored as
    // asset paths, never pointers.
    virtual void Serialize(FArchive& Ar) {}

    // After Serialize has loaded the data: resolve asset paths (dependencies are
    // already loaded) and create GPU resources.
    virtual bool PostLoad(FAssetManager& AssetManager, FRHI& RHI) { return true; }

private:
    friend class FAssetManager;
    FString AssetPath;
};
