#pragma once

#include "PrimitiveComponent.h"
#include "UObject/ObjectMacros.h"
#include "Components/StaticMeshComponent.generated.h"

class UStaticMesh;

UCLASS()
class UStaticMeshComponent : public UPrimitiveComponent
{
    GENERATED_BODY()
public:
    void SetStaticMesh(UStaticMesh* InStaticMesh) { StaticMesh = InStaticMesh; };
    UStaticMesh* GetStaticMesh() const { return StaticMesh; };
private:
    UStaticMesh* StaticMesh = nullptr;
};
