#pragma once

#include "Asset.h"
#include "UObject/ObjectMacros.h"

#include "Resources/Material.generated.h"

class UTexture2D;

UCLASS()
class UMaterial : public UAsset
{
    GENERATED_BODY()

public:
    void SetBaseColorTexture(UTexture2D* InTexture) { BaseColorTexture = InTexture; }
    UTexture2D* GetBaseColorTexture() const { return BaseColorTexture; }

private:
    UTexture2D* BaseColorTexture = nullptr;
};