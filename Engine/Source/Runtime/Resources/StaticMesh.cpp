#include "StaticMesh.h"

#include "RHI/RHI.h"
#include "Resources/AssetManager.h"
#include "Resources/Material.h"
#include "Serialization/Archive.h"

#include <limits>
#include <utility>

FArchive& operator<<(FArchive& Ar, FStaticMeshSection& Section)
{
    Ar << Section.FirstIndex << Section.IndexCount << Section.MaterialIndex << Section.MaterialName;
    return Ar;
}

void UStaticMesh::Serialize(FArchive& Ar)
{
    if (Ar.IsSaving())
    {
        TArray<FVertexSimple> Vertices = SourceVertices;
        TArray<uint32> Indices = RenderData.Indices;
        TArray<FStaticMeshSection> Sections = RenderData.Sections;
        TArray<FString> MaterialPaths = RenderData.MaterialAssetPaths;

        // Prefer the material's current asset path (it may have been renamed).
        for (int32 Index = 0; Index < MaterialPaths.Num() && Index < RenderData.Materials.Num(); ++Index)
        {
            if (const UMaterial* Material = RenderData.Materials[Index])
            {
                MaterialPaths[Index] = Material->GetAssetPath();
            }
        }

        bool bHasVertexNormals = RenderData.bHasVertexNormals;
        Ar << Vertices << Indices << Sections << MaterialPaths << bHasVertexNormals;
    }
    else
    {
        PendingLoadData = FStaticMeshBuildData();
        Ar << PendingLoadData.Vertices << PendingLoadData.Indices << PendingLoadData.Sections << PendingLoadData.MaterialAssetPaths << PendingLoadData.bHasVertexNormals;
    }
}

bool UStaticMesh::PostLoad(FAssetManager& AssetManager, FRHI& RHI)
{
    PendingLoadData.Materials.Reset();

    for (const FString& MaterialPath : PendingLoadData.MaterialAssetPaths)
    {
        PendingLoadData.Materials.Add(AssetManager.Find<UMaterial>(MaterialPath));
    }

    return Initialize(RHI, std::move(PendingLoadData));
}

bool UStaticMesh::Initialize(FRHI& Renderer, FStaticMeshBuildData&& SourceData)
{
    if (RenderData.VertexBuffer != nullptr || RenderData.IndexBuffer != nullptr || !RenderData.Positions.IsEmpty() || !RenderData.Indices.IsEmpty())
    {
        return false;
    }

    return Reinitialize(Renderer, std::move(SourceData));
}

bool UStaticMesh::Reinitialize(FRHI& Renderer, FStaticMeshBuildData&& SourceData)
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

    FRHIBuffer* NewVertexBuffer = Renderer.CreateVertexBuffer(SourceData.Vertices.GetData(), static_cast<UINT>(VertexByteWidth));

    if (NewVertexBuffer == nullptr)
    {
        return false;
    }

    FRHIBuffer* NewIndexBuffer = Renderer.CreateIndexBuffer(SourceData.Indices.GetData(), static_cast<UINT>(SourceData.Indices.Num()));

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

    SourceVertices = SourceData.Vertices;

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
        if (GRHI != nullptr)
        {
            GRHI->ReleaseVertexBuffer(RenderData.VertexBuffer);
        }
        RenderData.VertexBuffer = nullptr;
    }

    if (RenderData.IndexBuffer != nullptr)
    {
        if (GRHI != nullptr)
        {
            GRHI->ReleaseIndexBuffer(RenderData.IndexBuffer);
        }
        RenderData.IndexBuffer = nullptr;
    }
}