
#include "Actors/StaticMeshActor.h"

AStaticMeshActor::AStaticMeshActor()
{
    StaticMeshComponent = CreateComponent<UStaticMeshComponent>(FName("StaticMeshComponent"));

    if (StaticMeshComponent != nullptr)
    {
        SetRootComponent(StaticMeshComponent);
    }
}
