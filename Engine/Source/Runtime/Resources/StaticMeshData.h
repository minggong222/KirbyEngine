#pragma once

#include <d3d11.h>

#include "Core/STL.h"
#include "Core/Math/Box.h"
#include "FVertexSimple.h"

class UMaterial;

struct FStaticMeshSection
{
    uint32 FirstIndex = 0;
    uint32 IndexCount = 0;
    uint32 MaterialIndex = 0;
    FString MaterialName;
};

struct FStaticMeshBuildData
{
    TArray<FVertexSimple> Vertices;
    TArray<uint32> Indices;
    TArray<FStaticMeshSection> Sections;
    TArray<UMaterial*> Materials;
    TArray<FString> MaterialAssetPaths;
    bool bHasVertexNormals = false;
};

struct FStaticMeshRenderData
{
    TArray<FVector> Positions;
    TArray<uint32> Indices;
    TArray<FStaticMeshSection> Sections;
    TArray<UMaterial*> Materials;
    TArray<FString> MaterialAssetPaths;

    FBox LocalBounds;

    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;

    uint32 VertexCount = 0;
    uint32 IndexCount = 0;

    bool bHasVertexNormals = false;
};