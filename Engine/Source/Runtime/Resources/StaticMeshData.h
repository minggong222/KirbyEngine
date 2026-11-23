#pragma once
#include <d3d11.h>
#include "Core/STL.h"
#include "Core/Math/Box.h"
#include "FVertexSimple.h"

struct FStaticMeshBuildData
{
    TArray<FVertexSimple> Vertices;
    TArray<uint32> Indices;
};

struct FStaticMeshRenderData
{
    TArray<FVector> Positions;
    TArray<uint32> Indices;
    
    FBox LocalBounds;
    
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    
    uint32 VertexCount = 0;
    uint32 IndexCount = 0;
};