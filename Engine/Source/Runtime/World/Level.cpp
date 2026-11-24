#include "World/Level.h"

#include "Actors/Actor.h"
#include "World/World.h"

ULevel::~ULevel()
{
    while (!Actors.IsEmpty())
    {
        AActor* Actor = Actors.Pop();

        if (Actor != nullptr)
        {
            Actor->EndPlay();
            delete Actor;
        }
    }
}

UWorld* ULevel::GetWorld() const
{
    return Cast<UWorld>(GetOuter());
}

bool ULevel::AddActor(AActor* Actor)
{
    if (Actor == nullptr || Actor->GetOuter() != this || Actors.Contains(Actor))
    {
        return false;
    }

    Actors.Add(Actor);

    return true;
}

bool ULevel::RemoveActor(AActor* Actor)
{
    return Actor != nullptr && Actors.RemoveSingle(Actor);
}

AActor* ULevel::FindActorByName(FName InName) const
{
    for (AActor* Actor : Actors)
    {
        if (Actor != nullptr && Actor->GetFName() == InName)
        {
            return Actor;
        }
    }

    return nullptr;
}
