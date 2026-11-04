#pragma once
#include "FVertexSimple.h"
#include "PrimitiveComponent.h"

class USphereComp : public UPrimitiveComponent
{
	DECLARE_CLASS(USphereComp, UPrimitiveComponent);
public:
	USphereComp() = default;
	
    static TArray<FVertexSimple> CreateMeshVertices();
private:
};
