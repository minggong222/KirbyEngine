#pragma once

#include "Actors/Actor.h"
#include "Components/CameraComponent.h"
#include "UObject/ObjectMacros.h"
#include "Actors/CameraActor.generated.h"

UCLASS()
class ACameraActor : public AActor
{
    GENERATED_BODY()

public:
    ACameraActor();

    UCameraComponent* GetCameraComponent() const { return CameraComponent; }

private:
    UCameraComponent* CameraComponent = nullptr;
};
