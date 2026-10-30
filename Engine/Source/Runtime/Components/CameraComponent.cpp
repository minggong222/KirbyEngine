#include "CameraComponent.h"

#include "Core/Math/MathUtility.h"

IMPLEMENT_CLASS(UCameraComponent, USceneComponent)

void UCameraComponent::RegisterProperties(UClass& ClassInfo)
{
    REGISTER_PROPERTY_META(ClassInfo, UCameraComponent, FieldOfView, "Field of View", "Camera", Property_Visible | Property_Editable);
    REGISTER_PROPERTY_META(ClassInfo, UCameraComponent, AspectRatio, "Aspect Ratio", "Camera", Property_Visible | Property_Editable);
    REGISTER_PROPERTY_META(ClassInfo, UCameraComponent, NearClipPlane, "Near Clip Plane", "Camera", Property_Visible | Property_Editable);
    REGISTER_PROPERTY_META(ClassInfo, UCameraComponent, FarClipPlane, "Far Clip Plane", "Camera", Property_Visible | Property_Editable);
    REGISTER_PROPERTY_META(ClassInfo, UCameraComponent, bIsOrthogonal, "Orthographic", "Camera", Property_Visible | Property_Editable);
    REGISTER_PROPERTY_META(ClassInfo, UCameraComponent, OrthoWidth, "Orthographic Width", "Camera", Property_Visible | Property_Editable);
    REGISTER_PROPERTY_META(ClassInfo, UCameraComponent, MoveSpeed, "Move Speed", "Camera", Property_Visible | Property_Editable);
    REGISTER_PROPERTY_META(ClassInfo, UCameraComponent, MouseSensitivity, "Mouse Sensitivity", "Camera", Property_Visible | Property_Editable);
    REGISTER_PROPERTY_META(ClassInfo, UCameraComponent, WheelSpeed, "Wheel Speed", "Camera", Property_Visible | Property_Editable);
}

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
