#pragma once
#include "SceneComponent.h"
#include "Core/Math/Box.h"
#include "Resources/StaticMeshData.h"

class UPrimitiveComponent : public USceneComponent
{
    DECLARE_CLASS(UPrimitiveComponent, USceneComponent)
public:
    void SetStaticMeshData(const FStaticMeshData* InMeshData) { StaticMeshData = InMeshData; }
    const FStaticMeshData* GetStaticMeshData() const { return StaticMeshData; }
private:
    const FStaticMeshData* StaticMeshData = nullptr;
};
