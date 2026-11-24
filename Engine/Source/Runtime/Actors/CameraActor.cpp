#include "Actors/CameraActor.h"

ACameraActor::ACameraActor()
{
    CameraComponent = CreateComponent<UCameraComponent>(FName("CameraComponent"));

    if (CameraComponent != nullptr)
    {
        CameraComponent->SetRelativeTransform(FTransform());
        SetRootComponent(CameraComponent);
    }
}
