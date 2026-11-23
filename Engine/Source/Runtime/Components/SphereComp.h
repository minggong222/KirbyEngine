#pragma once
#include "FVertexSimple.h"
#include "PrimitiveComponent.h"
#include "UObject/ObjectMacros.h"
#include "Components/SphereComp.generated.h"

UCLASS()
class USphereComp : public UPrimitiveComponent
{
	GENERATED_BODY()
public:
	USphereComp() = default;
	
    static TArray<FVertexSimple> CreateMeshVertices();
private:
};
