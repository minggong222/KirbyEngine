#include "World/World.h"

#include "Renderer/Scene.h"

UWorld::UWorld()
{
    PersistentLevel = NewObject<ULevel>(this, FName("PersistentLevel"));
    Scene = new FScene();
}

UWorld::~UWorld()
{
    EndPlay();
    FlushPendingDestroy();

    delete Scene;
    Scene = nullptr;

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
    if (PersistentLevel == nullptr || !IsValid(Actor) || !PersistentLevel->GetActors().Contains(Actor))
    {
        return false;
    }

    Actor->EndPlay();
    PersistentLevel->RemoveActor(Actor);

    Actor->MarkAsGarbage();

    for (UActorComponent* Component : Actor->GetComponents())
    {
        if (Component != nullptr)
        {
            Component->MarkAsGarbage();
        }
    }

    PendingDestroyActors.Add(Actor);
    return true;
}

void UWorld::FlushPendingDestroy()
{
    // Swap first so destructors that destroy more actors queue them for the next flush.
    TArray<AActor*> ActorsToDelete;
    std::swap(ActorsToDelete, PendingDestroyActors);

    for (AActor* Actor : ActorsToDelete)
    {
        delete Actor;
    }
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
    if (PersistentLevel != nullptr)
    {
        const bool bGameplayTick = bHasBegunPlay && WorldType != EWorldType::Editor;

        const TArray<AActor*> CurrentActors = PersistentLevel->GetActors();

        for (AActor* Actor : CurrentActors)
        {
            if (!IsValid(Actor) || !PersistentLevel->GetActors().Contains(Actor))
            {
                continue;
            }

            if (bGameplayTick || Actor->ShouldTickIfViewportsOnly())
            {
                Actor->Tick(DeltaTime);
            }
        }
    }

    FlushPendingDestroy();
}

void UWorld::SendAllEndOfFrameUpdates()
{
    if (Scene != nullptr)
    {
        Scene->UpdatePrimitives(*this);
    }
}
