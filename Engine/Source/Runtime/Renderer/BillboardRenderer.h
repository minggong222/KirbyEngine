#pragma once

#include "Core/Math/Math.h"

class URenderer;
struct ID3D11ShaderResourceView;

class FBillboardRenderer
{
public:
    void DrawWorldBillboard(URenderer& Renderer, const FVector& Center, const FVector& CameraRight, const FVector& CameraUp, const FVector2D& WorldSize, const FVector4& Color, const FMatrix& View, const FMatrix& Projection, ID3D11ShaderResourceView* Texture = nullptr) const;

    void DrawScreenAlignedQuad(URenderer& Renderer, const FVector& Center, const FVector& CameraPosition, const FVector& CameraRight, const FVector& CameraUp, const FVector2D& PixelSize, uint32 ViewportHeight, const FVector4& Color, const FMatrix& View, const FMatrix& Projection) const;

    static float GetWorldUnitsPerPixel(const FVector& WorldPosition, const FVector& CameraPosition, const FMatrix& Projection, uint32 ViewportHeight);
};
