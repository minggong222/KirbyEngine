#pragma once

#include "Core/Math/Math.h"
#include "Components/CameraComponent.h"

enum class EEditorViewportType : uint8
{
    Perspective,
    Front,
    Right,
    Top
};

struct FEditorViewportPane
{
    EEditorViewportType Type = EEditorViewportType::Perspective;

    float X = 0.0f;
    float Y = 0.0f;
    float Width = 0.0f;
    float Height = 0.0f;

    FMatrix View = FMatrix::Identity;
    FMatrix Projection = FMatrix::Identity;

    FVector CameraPosition = FVector::Zero;
    FVector CameraRight = FVector::Right;
    FVector CameraUp = FVector::Up;
};

class FEditorViewportClient
{
public:
    void Update(FEditorViewportPane& Pane, UCameraComponent* PerspectiveCamera) const
    {
        if (Pane.Width <= 0.0f || Pane.Height <= 0.0f)
        {
            return;
        }

        const float AspectRatio = Pane.Width / Pane.Height;

        if (Pane.Type == EEditorViewportType::Perspective)
        {
            if (PerspectiveCamera == nullptr)
            {
                return;
            }

            const FTransform CameraTransform = PerspectiveCamera->GetTransform();

            Pane.View = PerspectiveCamera->GetViewMatrix();
            Pane.Projection = PerspectiveCamera->GetProjectionMatrix();
            Pane.CameraPosition = CameraTransform.Location;
            Pane.CameraRight = CameraTransform.GetRight();
            Pane.CameraUp = CameraTransform.GetUp();

            return;
        }

        FVector Position = FVector::Zero;
        FVector Target = FVector::Zero;
        FVector Up = FVector::Up;

        switch (Pane.Type)
        {
        case EEditorViewportType::Front:
            Position = FVector(1000.0f, 0.0f, 0.0f);
            Up = FVector(0.0f, 0.0f, 1.0f);
            break;

        case EEditorViewportType::Right:
            Position = FVector(0.0f, 1000.0f, 0.0f);
            Up = FVector(0.0f, 0.0f, 1.0f);
            break;

        case EEditorViewportType::Top:
            Position = FVector(0.0f, 0.0f, 1000.0f);
            Up = FVector(0.0f, 1.0f, 0.0f);
            break;

        default:
            return;
        }

        FVector Direction = Target - Position;
        Direction.Normalize();

        FVector Right = Up.Cross(Direction);

        if (Right.LengthSquared() <= 1.0e-8f)
        {
            return;
        }

        Right.Normalize();

        FVector CameraUp = Direction.Cross(Right);
        CameraUp.Normalize();

        constexpr float OrthoHeight = 1000.0f;
        const float OrthoWidth = OrthoHeight * AspectRatio;

        Pane.View = FMatrix::CreateLookAt(Position, Target, CameraUp);
        Pane.Projection = FMatrix::CreateOrthographic(OrthoWidth, OrthoHeight, 0.1f, 20000.0f);
        Pane.CameraPosition = Position;
        Pane.CameraRight = Right;
        Pane.CameraUp = CameraUp;
    }
};