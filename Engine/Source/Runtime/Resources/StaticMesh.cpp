#include "StaticMesh.h"

#include "Renderer/Renderer.h"
IMPLEMENT_CLASS_NO_PROPERTIES(UStaticMesh, UAsset);

bool UStaticMesh::Initialize(URenderer& Renderer, TArray<FVertexSimple>&& SourceVertices)
{
    if (SourceVertices.Num() < 3 || SourceVertices.Num() % 3 != 0)
    {
        return false;
    }
    
    if (RenderData.VertexBuffer != nullptr || !RenderData.Positions.IsEmpty())
    {
        return false;
    }
    const uint64 ByteWidth = sizeof(FVertexSimple) * static_cast<uint64>(SourceVertices.Num());
    if (ByteWidth > (std::numeric_limits<UINT>::max)())
    {
        return false;
    }
    
    RenderData.Positions.Reserve(SourceVertices.Num());
    for (const FVertexSimple& Vertex : SourceVertices)
    {
        const FVector Position(Vertex.x, Vertex.y, Vertex.z);
        RenderData.Positions.Add(Position);
        RenderData.LocalBounds.Add(Position);
    }
    
    RenderData.VertexCount = static_cast<uint32>(SourceVertices.Num());
    RenderData.VertexBuffer = Renderer.CreateVertexBuffer(SourceVertices.GetData(), static_cast<UINT>(ByteWidth));
    
    if (RenderData.VertexBuffer == nullptr)
    {
        RenderData.Positions.Reset();
        RenderData.LocalBounds.Reset();
        RenderData.VertexCount = 0;

        return false;
    }

    return true;
}

UStaticMesh::~UStaticMesh()
{
    if (RenderData.VertexBuffer != nullptr)
    {
        RenderData.VertexBuffer->Release();
        RenderData.VertexBuffer = nullptr;
    }
}
