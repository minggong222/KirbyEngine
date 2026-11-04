#include "SphereComp.h"

IMPLEMENT_CLASS_NO_PROPERTIES(USphereComp, UPrimitiveComponent)

USphereComp::USphereComp()
{
    FBox Bounds;
    
    for (const FVertexSimple& Vertex : Vertices)
    {
        Bounds.Add(FVector(Vertex.x, Vertex.y, Vertex.z));
    }
    SetLocalBounds(Bounds);
}
