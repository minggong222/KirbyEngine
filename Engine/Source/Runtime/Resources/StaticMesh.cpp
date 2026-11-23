#include "StaticMesh.h"

#include "Renderer/Renderer.h"
#include <limits>
bool UStaticMesh::Initialize(URenderer& Renderer, FStaticMeshBuildData&& SourceData)
{
    if (SourceData.Vertices.Num() < 3 || SourceData.Indices.Num() < 3 || SourceData.Indices.Num() % 3 != 0)
    {
        return false;
    }
    
    if (RenderData.VertexBuffer != nullptr || RenderData.IndexBuffer != nullptr || !RenderData.Positions.IsEmpty() || !RenderData.Indices.IsEmpty())
    {
        return false;
    }
    
    const uint64 VertexByteWidth = sizeof(FVertexSimple) * static_cast<uint64>(SourceData.Vertices.Num());
    const uint64 IndexByteWidth = sizeof(uint32) * static_cast<uint64>(SourceData.Indices.Num());
    
    if (VertexByteWidth > (std::numeric_limits<UINT>::max)() || IndexByteWidth > (std::numeric_limits<UINT>::max)())
    {
        return false;
    }
    
    for (uint32 Index : SourceData.Indices)
    {
        if (Index >= static_cast<uint32>(SourceData.Vertices.Num()))
        {
            return false;
        }
    }
    
    ID3D11Buffer* NewVertexBuffer = Renderer.CreateVertexBuffer(SourceData.Vertices.GetData(), static_cast<UINT>(VertexByteWidth));
    
    if (NewVertexBuffer == nullptr)
    {
        return false;
    }
    
    ID3D11Buffer* NewIndexBuffer = Renderer.CreateIndexBuffer(SourceData.Indices.GetData(), static_cast<UINT>(SourceData.Indices.Num()));
    
    if (NewIndexBuffer == nullptr)
    {
        Renderer.ReleaseVertexBuffer(NewVertexBuffer);
        return false;
    }
    
    TArray<FVector> Positions;
    Positions.Reserve(SourceData.Vertices.Num());
    
    FBox LocalBounds;
    
    for (const FVertexSimple& Vertex : SourceData.Vertices)
    {
        const FVector Position(Vertex.x, Vertex.y, Vertex.z);
        Positions.Add(Position);
        LocalBounds.Add(Position);
    }

    RenderData.Positions = std::move(Positions);
    RenderData.Indices = std::move(SourceData.Indices);
    RenderData.LocalBounds = LocalBounds;

    RenderData.VertexBuffer = NewVertexBuffer;
    RenderData.IndexBuffer = NewIndexBuffer;

    RenderData.VertexCount = static_cast<uint32>(SourceData.Vertices.Num());
    RenderData.IndexCount = static_cast<uint32>(RenderData.Indices.Num());

    return true;
}

UStaticMesh::~UStaticMesh()
{
    if (RenderData.VertexBuffer != nullptr)
    {
        RenderData.VertexBuffer->Release();
        RenderData.VertexBuffer = nullptr;
    }

    if (RenderData.IndexBuffer != nullptr)
    {
        RenderData.IndexBuffer->Release();
        RenderData.IndexBuffer = nullptr;
    }
}
