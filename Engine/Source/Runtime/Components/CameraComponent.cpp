#include "CameraComponent.h"

#include "Core/Math/MathUtility.h"

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
    const FTransform CameraTransform = GetTransform();
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
