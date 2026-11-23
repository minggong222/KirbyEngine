#pragma once

#include "FVertexSimple.h"
#include "PrimitiveComponent.h"
#include "UObject/ObjectMacros.h"
#include "Components/PlaneComp.generated.h"

UCLASS()
class UPlaneComp : public UPrimitiveComponent
{
    GENERATED_BODY()

public:
    static FStaticMeshBuildData CreateMeshData();
};