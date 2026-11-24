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
    void SetStaticMesh(UStaticMesh* InStaticMesh) { StaticMesh = InStaticMesh; }
    UStaticMesh* GetStaticMesh() const { return StaticMesh; }
    FBox CalcBounds() const override;

    void SetMaterial(UMaterial* InMaterial) { Material = InMaterial; }
    UMaterial* GetMaterial() const { return Material; }

private:
    UStaticMesh* StaticMesh = nullptr;
    UMaterial* Material = nullptr;
};