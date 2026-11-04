#pragma once
#include <d3d11.h>
#include "Core/STL.h"
#include "Core/Math/Box.h"

struct FStaticMeshRenderData
{
    TArray<FVector> Positions;
    FBox LocalBounds;
    ID3D11Buffer* VertexBuffer = nullptr;
    uint32 VertexCount = 0;
};