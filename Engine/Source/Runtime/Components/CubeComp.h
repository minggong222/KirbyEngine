#pragma once
#include "FVertexSimple.h"
#include "PrimitiveComponent.h"

class UCubeComp : public UPrimitiveComponent
{
    DECLARE_CLASS(UCubeComp, UPrimitiveComponent)
public:
    UCubeComp() = default;
    
    static TArray<FVertexSimple> CreateMeshVertices();
private:

};
