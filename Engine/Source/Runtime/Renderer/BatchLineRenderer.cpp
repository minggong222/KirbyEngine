#include "Core/Precompiled.h"
#include "Renderer/BatchLineRenderer.h"
#include "RHI/RHI.h"

#include <algorithm>
#include <cmath>

void FBatchLineRenderer::BeginFrame()
{
    Vertices.Reset();
    Indices.Reset();
}

void FBatchLineRenderer::AddLine(const FVector& Start, const FVector& End, const FVector4& Color)
{
    AddLine(Start, End, Color, Color);
}

void FBatchLineRenderer::AddLine(const FVector& Start, const FVector& End, const FVector4& StartColor, const FVector4& EndColor)
{
    const uint32 FirstVertex = static_cast<uint32>(Vertices.Num());
    Vertices.Add({Start.x, Start.y, Start.z, StartColor.x, StartColor.y, StartColor.z, StartColor.w, 0.0f, 0.0f});
    Vertices.Add({End.x, End.y, End.z, EndColor.x, EndColor.y, EndColor.z, EndColor.w, 0.0f, 0.0f});
    Indices.Add(FirstVertex);
    Indices.Add(FirstVertex + 1);
}

const FVector4& FBatchLineRenderer::GetAxisColor(int32 AxisIndex)
{
    static const FVector4 AxisColors[3] =
    {
        FVector4(0.80f, 0.15f, 0.00f, 1.0f), // X
        FVector4(0.40f, 0.67f, 0.00f, 1.0f), // Y
        FVector4(0.17f, 0.49f, 0.93f, 1.0f), // Z
    };

    return AxisColors[AxisIndex < 0 || AxisIndex > 2 ? 0 : AxisIndex];
}

void FBatchLineRenderer::AddBox(const FBox& Bounds, const FVector4& Color)
{
    if (!Bounds.IsValid()) return;
    const FVector& Min = Bounds.GetMin();
    const FVector& Max = Bounds.GetMax();
    const FVector Corners[] =
    {
        FVector(Min.x, Min.y, Min.z), FVector(Max.x, Min.y, Min.z), FVector(Max.x, Max.y, Min.z), FVector(Min.x, Max.y, Min.z),
        FVector(Min.x, Min.y, Max.z), FVector(Max.x, Min.y, Max.z), FVector(Max.x, Max.y, Max.z), FVector(Min.x, Max.y, Max.z)
    };
    const uint32 FirstVertex = static_cast<uint32>(Vertices.Num());
    for (const FVector& Corner : Corners) Vertices.Add({Corner.x, Corner.y, Corner.z, Color.x, Color.y, Color.z, Color.w, 0.0f, 0.0f});
    constexpr uint32 Edges[] = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};
    for (uint32 Edge : Edges) Indices.Add(FirstVertex + Edge);
}

void FBatchLineRenderer::AddGrid(const FGridDrawParams& Params)
{
    if (!std::isfinite(Params.BaseSpacing) || Params.BaseSpacing <= 0.0f || !std::isfinite(Params.PlaneOffset))
    {
        return;
    }

    // Plane basis: U/V are the world axes the grid spans, N is the plane normal axis.
    int32 AxisU = 0;
    int32 AxisV = 1;
    int32 AxisN = 2;

    switch (Params.Plane)
    {
    case EGridPlane::YZ: AxisU = 1; AxisV = 2; AxisN = 0; break;
    case EGridPlane::XZ: AxisU = 0; AxisV = 2; AxisN = 1; break;
    default: break;
    }

    const float ViewCoords[3] = {Params.ViewOrigin.x, Params.ViewOrigin.y, Params.ViewOrigin.z};

    if (!std::isfinite(ViewCoords[0]) || !std::isfinite(ViewCoords[1]) || !std::isfinite(ViewCoords[2]))
    {
        return;
    }

    const auto MakePoint = [&](float U, float V)
    {
        float Coords[3] = {};
        Coords[AxisU] = U;
        Coords[AxisV] = V;
        Coords[AxisN] = Params.PlaneOffset;
        return FVector(Coords[0], Coords[1], Coords[2]);
    };

    const int32 Interval = Params.MajorInterval >= 2 ? Params.MajorInterval : 10;
    constexpr int32 MaxLinesPerSide = 100;

    // Level of detail: grow the cell size by the major interval as the camera moves away
    // (perspective) or zooms out (ortho), so the on-screen density stays roughly constant.
    float Spacing = Params.BaseSpacing;
    float Extent = 0.0f;

    if (Params.bPerspective)
    {
        const float Height = (std::max)(std::fabs(ViewCoords[AxisN] - Params.PlaneOffset), 1.0e-3f);

        while (Spacing < Height * 0.02f && Spacing < 1.0e6f)
        {
            Spacing *= static_cast<float>(Interval);
        }

        // Visible radius follows the camera height (like Unreal's grid, which fades out
        // well before the horizon); MaxExtent caps it.
        Extent = (std::max)(Height * 10.0f, Spacing * 10.0f);

        if (std::isfinite(Params.MaxExtent) && Params.MaxExtent > 0.0f)
        {
            Extent = (std::min)(Extent, (std::max)(Params.MaxExtent, Spacing * 2.0f));
        }
    }
    else
    {
        const float VisibleSize = (std::max)(Params.OrthoVisibleSize, 1.0e-3f);

        while (Spacing < VisibleSize / 40.0f && Spacing < 1.0e6f)
        {
            Spacing *= static_cast<float>(Interval);
        }

        Extent = VisibleSize * 0.75f;
    }

    if (!std::isfinite(Extent) || Extent <= 0.0f)
    {
        return;
    }

    Extent = (std::min)(Extent, Spacing * static_cast<float>(MaxLinesPerSide));

    // Center on the view, snapped to the major spacing so lines don't swim while moving.
    const float MajorSpacing = Spacing * static_cast<float>(Interval);
    const float CenterU = std::floor(ViewCoords[AxisU] / MajorSpacing) * MajorSpacing;
    const float CenterV = std::floor(ViewCoords[AxisV] / MajorSpacing) * MajorSpacing;
    const float FocusU = ViewCoords[AxisU];
    const float FocusV = ViewCoords[AxisV];
    const int32 LineCount = static_cast<int32>(std::ceil(Extent / Spacing));

    const FVector4 MinorColor(0.45f, 0.45f, 0.45f, 0.16f);
    const FVector4 MajorColor(0.55f, 0.55f, 0.55f, 0.38f);

    // Perspective grids fade out toward the edge; lines are split into segments so the
    // fade can vary along each line.
    const int32 Segments = Params.bPerspective ? 16 : 1;

    const auto Fade = [&](float U, float V, FVector4 Color)
    {
        if (Params.bPerspective)
        {
            const float DU = U - FocusU;
            const float DV = V - FocusV;
            const float T = (std::min)(std::sqrt(DU * DU + DV * DV) / Extent, 1.0f);
            const float S = T <= 0.25f ? 0.0f : (T - 0.25f) / 0.75f;
            Color.w *= 1.0f - S * S * (3.0f - 2.0f * S);
        }

        return Color;
    };

    const auto AddGridLine = [&](float U0, float V0, float U1, float V1, const FVector4& Color)
    {
        for (int32 Segment = 0; Segment < Segments; ++Segment)
        {
            const float A = static_cast<float>(Segment) / static_cast<float>(Segments);
            const float B = static_cast<float>(Segment + 1) / static_cast<float>(Segments);
            const float SU = U0 + (U1 - U0) * A;
            const float SV = V0 + (V1 - V0) * A;
            const float EU = U0 + (U1 - U0) * B;
            const float EV = V0 + (V1 - V0) * B;

            AddLine(MakePoint(SU, SV), MakePoint(EU, EV), Fade(SU, SV, Color), Fade(EU, EV, Color));
        }
    };

    const auto LineColor = [&](float Coordinate, int32 AxisAlongLine, bool& bOutIsAxis)
    {
        const long long CellIndex = std::llround(Coordinate / Spacing);
        bOutIsAxis = CellIndex == 0;

        if (bOutIsAxis)
        {
            return GetAxisColor(AxisAlongLine);
        }

        return CellIndex % Interval == 0 ? MajorColor : MinorColor;
    };

    const float MinU = CenterU - static_cast<float>(LineCount) * Spacing;
    const float MaxU = CenterU + static_cast<float>(LineCount) * Spacing;
    const float MinV = CenterV - static_cast<float>(LineCount) * Spacing;
    const float MaxV = CenterV + static_cast<float>(LineCount) * Spacing;

    // Axis lines are queued last so they draw over the regular lines.
    struct FAxisLine
    {
        float U0, V0, U1, V1;
        FVector4 Color;
    };

    FAxisLine AxisLines[2];
    int32 AxisLineCount = 0;

    for (int32 Index = -LineCount; Index <= LineCount; ++Index)
    {
        const float Offset = static_cast<float>(Index) * Spacing;
        bool bIsAxis = false;

        // Line of constant U runs along V.
        const float U = CenterU + Offset;
        const FVector4 ColorU = LineColor(U, AxisV, bIsAxis);

        if (bIsAxis)
        {
            AxisLines[AxisLineCount++] = {0.0f, MinV, 0.0f, MaxV, ColorU};
        }
        else
        {
            AddGridLine(U, MinV, U, MaxV, ColorU);
        }

        // Line of constant V runs along U.
        const float V = CenterV + Offset;
        const FVector4 ColorV = LineColor(V, AxisU, bIsAxis);

        if (bIsAxis)
        {
            AxisLines[AxisLineCount++] = {MinU, 0.0f, MaxU, 0.0f, ColorV};
        }
        else
        {
            AddGridLine(MinU, V, MaxU, V, ColorV);
        }
    }

    for (int32 Index = 0; Index < AxisLineCount; ++Index)
    {
        const FAxisLine& Line = AxisLines[Index];
        AddGridLine(Line.U0, Line.V0, Line.U1, Line.V1, Line.Color);
    }

    // Perspective: also draw the normal axis (Z on the floor grid) as a vertical line through
    // the origin, faded like the grid by the origin's horizontal distance from the camera.
    if (Params.bPerspective)
    {
        const FVector4 NormalColor = Fade(0.0f, 0.0f, GetAxisColor(AxisN));

        if (NormalColor.w > 0.0f)
        {
            const auto MakeNormalPoint = [&](float N)
            {
                float Coords[3] = {};
                Coords[AxisN] = Params.PlaneOffset + N;
                return FVector(Coords[0], Coords[1], Coords[2]);
            };

            // Long enough to always reach the far plane, so it never ends on screen when looking up/down.
            constexpr float NormalAxisLength = 1.0e5f;
            AddLine(MakeNormalPoint(-NormalAxisLength), MakeNormalPoint(NormalAxisLength), NormalColor);
        }
    }
}

bool FBatchLineRenderer::Flush(FRHI& Renderer, const FMatrix& View, const FMatrix& Projection)
{
    if (Vertices.IsEmpty() || Indices.IsEmpty()) return true;
    const bool bDrawn = Renderer.DrawDynamicIndexedLines(Vertices.GetData(), static_cast<UINT>(Vertices.Num()), Indices.GetData(), static_cast<UINT>(Indices.Num()), View, Projection);
    Vertices.Reset();
    Indices.Reset();
    return bDrawn;
}
