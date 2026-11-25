#include "StaticMesh.h"

#include "Renderer/Renderer.h"
#include "Resources/Material.h"

#include <limits>
#include <utility>

bool UStaticMesh::Initialize(URenderer& Renderer, FStaticMeshBuildData&& SourceData)
{
    if (RenderData.VertexBuffer != nullptr || RenderData.IndexBuffer != nullptr || !RenderData.Positions.IsEmpty() || !RenderData.Indices.IsEmpty())
    {
        return false;
    }

    return Reinitialize(Renderer, std::move(SourceData));
}

bool UStaticMesh::Reinitialize(URenderer& Renderer, FStaticMeshBuildData&& SourceData)
{
    if (SourceData.Vertices.Num() < 3 || SourceData.Indices.Num() < 3 || SourceData.Indices.Num() % 3 != 0)
    {
        return false;
    }

    if (SourceData.Materials.Num() != SourceData.MaterialAssetPaths.Num())
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

    if (SourceData.Sections.IsEmpty())
    {
        FStaticMeshSection DefaultSection;
        DefaultSection.FirstIndex = 0;
        DefaultSection.IndexCount = static_cast<uint32>(SourceData.Indices.Num());
        DefaultSection.MaterialIndex = 0;
        SourceData.Sections.Add(DefaultSection);
    }

    uint32 ExpectedFirstIndex = 0;

    for (const FStaticMeshSection& Section : SourceData.Sections)
    {
        if (Section.IndexCount == 0 || Section.IndexCount % 3 != 0 || Section.FirstIndex != ExpectedFirstIndex)
        {
            return false;
        }

        if (Section.FirstIndex > static_cast<uint32>(SourceData.Indices.Num()) || Section.IndexCount > static_cast<uint32>(SourceData.Indices.Num()) - Section.FirstIndex)
        {
            return false;
        }

        if (!SourceData.Materials.IsEmpty() && Section.MaterialIndex >= static_cast<uint32>(SourceData.Materials.Num()))
        {
            return false;
        }

        ExpectedFirstIndex += Section.IndexCount;
    }

    if (ExpectedFirstIndex != static_cast<uint32>(SourceData.Indices.Num()))
    {
        return false;
    }

    const uint64 VertexByteWidth = sizeof(FVertexSimple) * static_cast<uint64>(SourceData.Vertices.Num());
    const uint64 IndexByteWidth = sizeof(uint32) * static_cast<uint64>(SourceData.Indices.Num());

    if (VertexByteWidth > (std::numeric_limits<UINT>::max)() || IndexByteWidth > (std::numeric_limits<UINT>::max)())
    {
        return false;
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

    TArray<FVector> NewPositions;
    NewPositions.Reserve(SourceData.Vertices.Num());

    FBox NewLocalBounds;

    for (const FVertexSimple& Vertex : SourceData.Vertices)
    {
        const FVector Position(Vertex.x, Vertex.y, Vertex.z);
        NewPositions.Add(Position);
        NewLocalBounds.Add(Position);
    }

    if (RenderData.VertexBuffer != nullptr)
    {
        Renderer.ReleaseVertexBuffer(RenderData.VertexBuffer);
    }

    if (RenderData.IndexBuffer != nullptr)
    {
        Renderer.ReleaseIndexBuffer(RenderData.IndexBuffer);
    }

    RenderData.Positions = std::move(NewPositions);
    RenderData.Indices = std::move(SourceData.Indices);
    RenderData.Sections = std::move(SourceData.Sections);
    RenderData.Materials = std::move(SourceData.Materials);
    RenderData.MaterialAssetPaths = std::move(SourceData.MaterialAssetPaths);
    RenderData.LocalBounds = NewLocalBounds;
    RenderData.VertexBuffer = NewVertexBuffer;
    RenderData.IndexBuffer = NewIndexBuffer;
    RenderData.VertexCount = static_cast<uint32>(SourceData.Vertices.Num());
    RenderData.IndexCount = static_cast<uint32>(RenderData.Indices.Num());
    RenderData.bHasVertexNormals = SourceData.bHasVertexNormals;

    return true;
}

void UStaticMesh::GetDependencies(TArray<UAsset*>& OutDependencies) const
{
    OutDependencies.Reset();

    for (UMaterial* Material : RenderData.Materials)
    {
        if (Material != nullptr)
        {
            OutDependencies.Add(static_cast<UAsset*>(Material));
        }
    }
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