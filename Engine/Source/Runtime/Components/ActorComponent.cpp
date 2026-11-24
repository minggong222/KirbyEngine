
#include "Components/ActorComponent.h"

#include "Actors/Actor.h"

AActor* UActorComponent::GetOwner() const
{
    return Cast<AActor>(GetOuter());
}
