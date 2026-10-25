#pragma once
#include "Core/SimpleMath.h"

using FVector2D = DirectX::SimpleMath::Vector2;
using FVector = DirectX::SimpleMath::Vector3;
using FVector4 = DirectX::SimpleMath::Vector4;
using FMatrix = DirectX::SimpleMath::Matrix;
using FQuat = DirectX::SimpleMath::Quaternion;
using FLinearColor = DirectX::SimpleMath::Color;
using FPlane = DirectX::SimpleMath::Plane;
using FRay = DirectX::SimpleMath::Ray;
using FIntRect = DirectX::SimpleMath::Rectangle;
using FViewport = DirectX::SimpleMath::Viewport;

namespace EngineAxis
{
    inline const FVector Forward = FVector(1.0f, 0.0f, 0.0f);
    inline const FVector Right = FVector(0.0f, 1.0f, 0.0f);
    inline const FVector Up = FVector(0.0f, 0.0f, 1.0f);
    inline const FVector Backward = FVector(-1.0f, 0.0f, 0.0f);
    inline const FVector Left = FVector(0.0f, -1.0f, 0.0f);
    inline const FVector Down = FVector(0.0f, 0.0f, -1.0f);
}
