#pragma once
#include "FVertexSimple.h"
#include "PrimitiveComponent.h"
#include "UObject/ObjectMacros.h"
#include "Components/CubeComp.generated.h"

UCLASS()
class UCubeComp : public UPrimitiveComponent
{
    GENERATED_BODY()
public:
    UCubeComp() = default;
    
    static FStaticMeshBuildData CreateMeshData();
private:

};
