#pragma once

#include "Asset.h"
#include "Resources/Texture2D.h"
#include "UObject/ObjectMacros.h"
#include "Resources/Material.generated.h"

UCLASS()
class UMaterial : public UAsset
{
    GENERATED_BODY()

public:
    void SetBaseColorTexture(UTexture2D* InTexture) { BaseColorTexture = InTexture; }
    UTexture2D* GetBaseColorTexture() const { return BaseColorTexture; }

    void GetDependencies(TArray<UAsset*>& OutDependencies) const override
    {
        OutDependencies.Reset();

        if (BaseColorTexture != nullptr)
        {
            OutDependencies.Add(static_cast<UAsset*>(BaseColorTexture));
        }
    }

private:
    UTexture2D* BaseColorTexture = nullptr;
};
