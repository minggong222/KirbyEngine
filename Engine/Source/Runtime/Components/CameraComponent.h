#pragma once
#include "SceneComponent.h"

class UCameraComponent : public USceneComponent
{
public:
    UCameraComponent();
    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    float GetMoveSpeed() const { return MoveSpeed; }
    float GetMouseSensitivity() const { return MouseSensitivity; }
private:
    float FieldOfView = 60.0f;
    float AspectRatio = 16.0f / 9.0f;
    float NearClipPlane = 0.1f;
    float FarClipPlane = 1000.0f;

    bool bIsOrthogonal = false;
    float OrthoWidth = 16.0f;

    float MoveSpeed = 10.0f;
    float MouseSensitivity = 0.05f;
    float WheelSpeed = 0.1f;
};
