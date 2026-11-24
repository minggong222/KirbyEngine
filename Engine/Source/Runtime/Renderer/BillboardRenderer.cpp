#include "Core/Precompiled.h"
#include "Renderer/BillboardRenderer.h"

#include "Core/Math/MathUtility.h"
#include "Renderer/Renderer.h"

float FBillboardRenderer::GetWorldUnitsPerPixel(const FVector& WorldPosition, const FVector& CameraPosition, const FMatrix& Projection, uint32 ViewportHeight)
{
    if (ViewportHeight == 0)
    {
        return 0.0f;
    }

    const float ProjectionScale = std::fabs(Projection.M[1][1]);

    if (!std::isfinite(ProjectionScale) || ProjectionScale <= EPSILON_1E_6)
    {
        return 0.0f;
    }

    if (std::fabs(Projection.M[3][3]) > 0.5f)
    {
        return 2.0f / (ProjectionScale * static_cast<float>(ViewportHeight));
    }

    const float Distance = (WorldPosition - CameraPosition).Length();

    if (!std::isfinite(Distance) || Distance <= EPSILON_1E_6)
    {
        return 0.0f;
    }

    return (2.0f * Distance) / (ProjectionScale * static_cast<float>(ViewportHeight));
}

void FBillboardRenderer::DrawWorldBillboard(URenderer& Renderer, const FVector& Center, const FVector& CameraRight, const FVector& CameraUp, const FVector2D& WorldSize, const FVector4& Color, const FMatrix& View, const FMatrix& Projection, ID3D11ShaderResourceView* Texture) const
{
    if (WorldSize.x <= 0.0f || WorldSize.y <= 0.0f)
    {
        return;
    }

    FVector Right = CameraRight;
    FVector Up = CameraUp;

    if (Right.LengthSquared() <= EPSILON_1E_8 || Up.LengthSquared() <= EPSILON_1E_8)
    {
        return;
    }

    Right.Normalize();
    Up.Normalize();

    const FVector HalfRight = Right * (WorldSize.x * 0.5f);
    const FVector HalfUp = Up * (WorldSize.y * 0.5f);

    const FVector TopLeft = Center - HalfRight + HalfUp;
    const FVector TopRight = Center + HalfRight + HalfUp;
    const FVector BottomRight = Center + HalfRight - HalfUp;
    const FVector BottomLeft = Center - HalfRight - HalfUp;

    const FVertexSimple Vertices[] =
    {
        {TopLeft.x, TopLeft.y, TopLeft.z, Color.x, Color.y, Color.z, Color.w, 0.0f, 0.0f},
        {TopRight.x, TopRight.y, TopRight.z, Color.x, Color.y, Color.z, Color.w, 1.0f, 0.0f},
        {BottomRight.x, BottomRight.y, BottomRight.z, Color.x, Color.y, Color.z, Color.w, 1.0f, 1.0f},

        {TopLeft.x, TopLeft.y, TopLeft.z, Color.x, Color.y, Color.z, Color.w, 0.0f, 0.0f},
        {BottomRight.x, BottomRight.y, BottomRight.z, Color.x, Color.y, Color.z, Color.w, 1.0f, 1.0f},
        {BottomLeft.x, BottomLeft.y, BottomLeft.z, Color.x, Color.y, Color.z, Color.w, 0.0f, 1.0f}
    };

    Renderer.DrawDynamicVertices(Vertices, ARRAYSIZE(Vertices), D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, FMatrix::Identity, View, Projection, Texture, Texture != nullptr, true);
}

void FBillboardRenderer::DrawScreenAlignedQuad(URenderer& Renderer, const FVector& Center, const FVector& CameraPosition, const FVector& CameraRight, const FVector& CameraUp, const FVector2D& PixelSize, uint32 ViewportHeight, const FVector4& Color, const FMatrix& View, const FMatrix& Projection) const
{
    const float WorldUnitsPerPixel = GetWorldUnitsPerPixel(Center, CameraPosition, Projection, ViewportHeight);

    if (WorldUnitsPerPixel <= 0.0f)
    {
        return;
    }

    const FVector2D WorldSize = PixelSize * WorldUnitsPerPixel;

    DrawWorldBillboard(Renderer, Center, CameraRight, CameraUp, WorldSize, Color, View, Projection);
}
