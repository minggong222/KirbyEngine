#pragma once

#include "Core/Math/Box.h"
#include "Renderer/FVertexSimple.h"

class FRHI;

class FBatchLineRenderer
{
public:
    void BeginFrame();
    void AddLine(const FVector& Start, const FVector& End, const FVector4& Color);
    void AddBox(const FBox& Bounds, const FVector4& Color);
    void AddGrid(float GridExtent, float GridSpacing, int32 MajorLineInterval, bool bShowGrid, bool bShowAxes);
    bool Flush(FRHI& Renderer, const FMatrix& View, const FMatrix& Projection);

private:
    TArray<FVertexSimple> Vertices;
    TArray<uint32> Indices;
};
