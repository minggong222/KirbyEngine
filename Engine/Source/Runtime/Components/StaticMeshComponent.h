#pragma once

#include "PrimitiveComponent.h"
#include "UObject/ObjectMacros.h"
#include "Components/StaticMeshComponent.generated.h"

class UStaticMesh;
class UMaterial;

UCLASS()
class UStaticMeshComponent : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    void SetStaticMesh(UStaticMesh* InStaticMesh);
    UStaticMesh* GetStaticMesh() const { return StaticMesh; }

    FBox CalcBounds() const override;

    void SetMaterial(int32 MaterialIndex, UMaterial* InMaterial);
    UMaterial* GetMaterial(int32 MaterialIndex) const;

    int32 GetMaterialCount() const;

    TArray<UMaterial*> GetMaterials() const;
    TArray<UMaterial*> GetMaterialOverrides() const;
    TArray<uint8> GetMaterialOverrideMask() const;

    void SetMaterials(const TArray<UMaterial*>& InMaterials);
    void SetMaterialOverrides(const TArray<UMaterial*>& InMaterials, const TArray<uint8>& InOverrideMask);

    bool HasMaterialOverride(int32 MaterialIndex) const;
    void ClearMaterialOverride(int32 MaterialIndex);

private:
    UStaticMesh* StaticMesh = nullptr;
    TArray<UMaterial*> Materials;
    TArray<uint8> MaterialOverrideMask;
};