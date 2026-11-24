#pragma once

#include "Actors/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ObjectMacros.h"
#include "Actors/StaticMeshActor.generated.h"

UCLASS()
class AStaticMeshActor : public AActor
{
    GENERATED_BODY()

public:
    AStaticMeshActor();

    UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }

private:
    UStaticMeshComponent* StaticMeshComponent = nullptr;
};
