#include "World/World.h"

UWorld::UWorld()
{
    PersistentLevel = NewObject<ULevel>(this, FName("PersistentLevel"));
}

UWorld::~UWorld()
{
    EndPlay();

    delete PersistentLevel;
    PersistentLevel = nullptr;
}

FName UWorld::MakeUniqueActorName(FName RequestedName, const UClass* ActorClass) const
{
    if (PersistentLevel == nullptr || ActorClass == nullptr)
    {
        return RequestedName;
    }

    const FString BaseName = RequestedName.IsNone() ? ActorClass->GetName() : RequestedName.GetPlainNameString();
    const FName BaseNameEntry(BaseName);

    FName Candidate = RequestedName.IsNone() ? BaseNameEntry : RequestedName;

    int32 InternalNumber = 2;

    while (PersistentLevel->FindActorByName(Candidate) != nullptr)
    {
        Candidate = FName(BaseNameEntry, InternalNumber++);
    }

    return Candidate;
}

bool UWorld::DestroyActor(AActor* Actor)
{
    if (PersistentLevel == nullptr || Actor == nullptr || !PersistentLevel->GetActors().Contains(Actor))
    {
        return false;
    }

    Actor->EndPlay();
    PersistentLevel->RemoveActor(Actor);
    delete Actor;

    return true;
}

void UWorld::BeginPlay()
{
    if (bHasBegunPlay || PersistentLevel == nullptr)
    {
        return;
    }

    bHasBegunPlay = true;

    const TArray<AActor*> CurrentActors = PersistentLevel->GetActors();

    for (AActor* Actor : CurrentActors)
    {
        if (Actor != nullptr && PersistentLevel->GetActors().Contains(Actor))
        {
            Actor->BeginPlay();
        }
    }
}

void UWorld::EndPlay()
{
    if (!bHasBegunPlay || PersistentLevel == nullptr)
    {
        return;
    }

    bHasBegunPlay = false;

    const TArray<AActor*> CurrentActors = PersistentLevel->GetActors();

    for (int32 Index = CurrentActors.Num() - 1; Index >= 0; --Index)
    {
        AActor* Actor = CurrentActors[Index];

        if (Actor != nullptr && PersistentLevel->GetActors().Contains(Actor))
        {
            Actor->EndPlay();
        }
    }
}

void UWorld::Tick(float DeltaTime)
{
    if (!bHasBegunPlay || PersistentLevel == nullptr)
    {
        return;
    }

    const TArray<AActor*> CurrentActors = PersistentLevel->GetActors();

    for (AActor* Actor : CurrentActors)
    {
        if (Actor != nullptr && PersistentLevel->GetActors().Contains(Actor))
        {
            Actor->Tick(DeltaTime);
        }
    }
}
