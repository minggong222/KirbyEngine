#pragma once

#include "Core/Math/Box.h"
#include "Renderer/FVertexSimple.h"

class FRHI;

// World plane a grid is laid on. Named by the two world axes it spans.
enum class EGridPlane : uint8
{
    XY, // floor (perspective, top view)
    YZ, // front view (looking down -X)
    XZ, // right view (looking down -Y)
};

// Editor-style grid: follows the view, picks its spacing from the camera distance / zoom,
// and colors the lines through the world origin with the axis colors.
struct FGridDrawParams
{
    EGridPlane Plane = EGridPlane::XY;
    float PlaneOffset = 0.0f;           // position of the plane along its normal axis
    FVector ViewOrigin = FVector::Zero; // camera position (perspective) or view center (ortho)
    bool bPerspective = true;
    float OrthoVisibleSize = 0.0f;      // larger side of the ortho view in world units
    float BaseSpacing = 1.0f;           // smallest cell size
    int32 MajorInterval = 10;           // every Nth line is a major line, and the LOD step
    float MaxExtent = 0.0f;             // perspective: cap on the drawn radius (0 = no cap)
};

class FBatchLineRenderer
{
public:
    void BeginFrame();
    void AddLine(const FVector& Start, const FVector& End, const FVector4& Color);
    void AddLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor);
    void AddBox(const FBox& Bounds, const FVector4& Color);
    void AddGrid(const FGridDrawParams& Params);
    bool Flush(FRHI& Renderer, const FMatrix& View, const FMatrix& Projection);

    // Unreal-style axis colors (sRGB).
    static const FVector4& GetAxisColor(int32 AxisIndex);

private:
    TArray<FVertexSimple> Vertices;
    TArray<uint32> Indices;
};
