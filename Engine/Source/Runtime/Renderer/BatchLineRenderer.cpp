#include "Core/Precompiled.h"
#include "Renderer/BatchLineRenderer.h"
#include "Renderer/Renderer.h"

void FBatchLineRenderer::BeginFrame()
{
    Vertices.Reset();
}

void FBatchLineRenderer::AddLine(const FVector& Start, const FVector& End, const FVector4& Color)
{
    Vertices.Add({Start.x, Start.y, Start.z, Color.x, Color.y, Color.z, Color.w, 0.0f, 0.0f});
    Vertices.Add({End.x, End.y, End.z, Color.x, Color.y, Color.z, Color.w, 0.0f, 0.0f});
}

void FBatchLineRenderer::AddBox(const FBox& Bounds, const FVector4& Color)
{
    if (!Bounds.IsValid())
    {
        return;
    }

    const FVector& Min = Bounds.GetMin();
    const FVector& Max = Bounds.GetMax();

    const FVector Corners[] =
    {
        FVector(Min.x, Min.y, Min.z),
        FVector(Max.x, Min.y, Min.z),
        FVector(Max.x, Max.y, Min.z),
        FVector(Min.x, Max.y, Min.z),

        FVector(Min.x, Min.y, Max.z),
        FVector(Max.x, Min.y, Max.z),
        FVector(Max.x, Max.y, Max.z),
        FVector(Min.x, Max.y, Max.z)
    };

    constexpr int32 Edges[][2] =
    {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    for (const auto& Edge : Edges)
    {
        AddLine(Corners[Edge[0]], Corners[Edge[1]], Color);
    }
}

void FBatchLineRenderer::AddGrid(float GridExtent, float GridSpacing, int32 MajorLineInterval, bool bShowGrid, bool bShowAxes)
{
    if (GridExtent <= 0.0f || !std::isfinite(GridExtent))
    {
        return;
    }

    if (bShowGrid && GridSpacing > 0.0f && std::isfinite(GridSpacing))
    {
        const int32 LineCount = static_cast<int32>(std::floor(GridExtent / GridSpacing));

        if (LineCount <= 1000)
        {
            for (int32 Index = -LineCount; Index <= LineCount; ++Index)
            {
                if (Index == 0)
                {
                    continue;
                }

                const float Offset = static_cast<float>(Index) * GridSpacing;
                const bool bMajor = MajorLineInterval > 0 && std::abs(Index) % MajorLineInterval == 0;

                const FVector4 Color = bMajor
                    ? FVector4(0.55f, 0.58f, 0.63f, 0.38f)
                    : FVector4(0.45f, 0.48f, 0.52f, 0.18f);

                AddLine(FVector(-GridExtent, Offset, 0.0f), FVector(GridExtent, Offset, 0.0f), Color);
                AddLine(FVector(Offset, -GridExtent, 0.0f), FVector(Offset, GridExtent, 0.0f), Color);
            }
        }
    }

    if (bShowAxes)
    {
        AddLine(FVector(-GridExtent, 0.0f, 0.0f), FVector(GridExtent, 0.0f, 0.0f), FVector4(1.0f, 0.15f, 0.15f, 1.0f));
        AddLine(FVector(0.0f, -GridExtent, 0.0f), FVector(0.0f, GridExtent, 0.0f), FVector4(0.15f, 1.0f, 0.25f, 1.0f));
        AddLine(FVector(0.0f, 0.0f, -10.0f), FVector(0.0f, 0.0f, 20.0f), FVector4(0.2f, 0.45f, 1.0f, 1.0f));
    }
}

bool FBatchLineRenderer::Flush(URenderer& Renderer, const FMatrix& View, const FMatrix& Projection)
{
    if (Vertices.IsEmpty())
    {
        return true;
    }

    const bool bDrawn = Renderer.DrawDynamicVertices(Vertices.GetData(), static_cast<UINT>(Vertices.Num()), D3D11_PRIMITIVE_TOPOLOGY_LINELIST, FMatrix::Identity, View, Projection, nullptr, false, true);

    Vertices.Reset();

    return bDrawn;
}
