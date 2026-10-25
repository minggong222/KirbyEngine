#include "CameraComponent.h"

UCameraComponent::UCameraComponent()
{
    SetTransform(FTransform(FVector(10.0f, 10.0f, 10.0f), FVector(0.0f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f)));
}

FMatrix UCameraComponent::GetViewMatrix() const
{
    return FMatrix::Identity;
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
    return FMatrix::Identity;
}
