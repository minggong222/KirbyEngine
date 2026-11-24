#include "CameraComponent.h"

#include "Core/Math/MathUtility.h"
#include <algorithm>
#include <cmath>

UCameraComponent::UCameraComponent()
{
    SetTransform(FTransform(
        FVector(-3.0f, 0.0f, 0.0f),
        FVector(0.0f, 0.0f, 0.0f),
        FVector(1.0f, 1.0f, 1.0f)
    ));
}

FMatrix UCameraComponent::GetViewMatrix() const
{
    const FTransform CameraTransform = GetWorldTransform();
    const FVector CameraPosition = CameraTransform.Location;
    const FVector Forward = CameraTransform.GetForward();
    const FVector Up = CameraTransform.GetUp();
    
    return FMatrix::CreateLookAt(CameraPosition, CameraPosition + Forward, Up);
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
    if (bIsOrthogonal)
    {
        const float OrthoHeight = OrthoWidth / AspectRatio;
        return FMatrix::CreateOrthographic(OrthoWidth, OrthoHeight, NearClipPlane, FarClipPlane);
    }
    
    const float FovRadians = FieldOfView * PI_PRECISE / 180.0f;
    
    return FMatrix::CreatePerspectiveFieldOfView(FovRadians, AspectRatio, NearClipPlane, FarClipPlane);
}

void UCameraComponent::SetAspectRatio(float InAspectRatio)
{
    if (std::isfinite(InAspectRatio) && InAspectRatio > 0.0f)
    {
        AspectRatio = InAspectRatio;
    }
}

void UCameraComponent::SetMouseSensitivity(float InSensitivity)
{
    if (std::isfinite(InSensitivity))
    {
        MouseSensitivity = std::clamp(InSensitivity, 0.001f, 1.0f);
    }
}
