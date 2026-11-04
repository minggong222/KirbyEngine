#include "CubeComp.h"

IMPLEMENT_CLASS_NO_PROPERTIES(UCubeComp, UPrimitiveComponent)

UCubeComp::UCubeComp()
{
    FBox Bounds;
    
    for (const FVertexSimple& Vertex : Vertices)
    {
        Bounds.Add(FVector(Vertex.x, Vertex.y, Vertex.z));
    }
    SetLocalBounds(Bounds);
}
