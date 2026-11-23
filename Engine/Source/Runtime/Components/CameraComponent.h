#pragma once
#include "SceneComponent.h"
#include "UObject/ObjectMacros.h"
#include "Components/CameraComponent.generated.h"

UCLASS()
class UCameraComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    UCameraComponent();
    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    float GetMoveSpeed() const { return MoveSpeed; }
    float GetMouseSensitivity() const { return MouseSensitivity; }
    
    UFUNCTION(Category="Camera")
    void SetAspectRatio(float InAspectRatio);
private:
    UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="Field of View"))
    float FieldOfView = 60.0f;

    UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="Aspect Ratio"))
    float AspectRatio = 16.0f / 9.0f;

    UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="Near Clip Plane"))
    float NearClipPlane = 0.1f;

    UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="Far Clip Plane"))
    float FarClipPlane = 1000.0f;

    UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="Orthographic"))
    bool bIsOrthogonal = false;

    UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="Orthographic Width"))
    float OrthoWidth = 16.0f;

    UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="Move Speed"))
    float MoveSpeed = 10.0f;

    UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="Mouse Sensitivity"))
    float MouseSensitivity = 0.05f;

    UPROPERTY(EditAnywhere, Category="Camera", meta=(DisplayName="Wheel Speed"))
    float WheelSpeed = 0.1f;
};
