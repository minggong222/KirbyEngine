#pragma once

#include "PrimitiveComponent.h"

class UStaticMesh;

class UStaticMeshComponent : public UPrimitiveComponent
{
    DECLARE_CLASS(UStaticMeshComponent, UPrimitiveComponent)
public:
    void SetStaticMesh(UStaticMesh* InStaticMesh) { StaticMesh = InStaticMesh; };
    UStaticMesh* GetStaticMesh() const { return StaticMesh; };
private:
    UStaticMesh* StaticMesh = nullptr;
};
