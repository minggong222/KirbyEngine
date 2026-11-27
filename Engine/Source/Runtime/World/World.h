#pragma once

#include "Actors/Actor.h"
#include "World/Level.h"
#include "World/WorldType.h"
#include "UObject/ObjectMacros.h"
#include "World/World.generated.h"

class FScene;

UCLASS()
class UWorld : public UObject
{
    GENERATED_BODY()

public:
    UWorld();
    virtual ~UWorld() override;

    ULevel* GetPersistentLevel() const { return PersistentLevel; }

    template<typename TActor>
    TActor* SpawnActor(FName InName = FName(), const FTransform& InTransform = FTransform())
    {
        static_assert(std::is_base_of_v<AActor, TActor>, "TActor must derive from AActor");

        if (PersistentLevel == nullptr)
        {
            return nullptr;
        }

        const FName ActorName = MakeUniqueActorName(InName, TActor::StaticClass());

        TActor* Actor = NewObject<TActor>(PersistentLevel, ActorName);

        if (Actor == nullptr)
        {
            return nullptr;
        }

        if (!PersistentLevel->AddActor(Actor))
        {
            delete Actor;
            return nullptr;
        }

        if (Actor->GetRootComponent() != nullptr && !Actor->SetActorTransform(InTransform))
        {
            PersistentLevel->RemoveActor(Actor);
            delete Actor;
            return nullptr;
        }

        if (bHasBegunPlay)
        {
            Actor->BeginPlay();
        }

        return Actor;
    }

    // Removes the actor from the level and marks it (and its components) as garbage.
    // The memory is freed later by FlushPendingDestroy, so pointers held elsewhere
    // during this frame stay safe to compare and IsValid() reports false.
    bool DestroyActor(AActor* Actor);

    // Deletes actors destroyed since the last flush. Called at the end of Tick.
    void FlushPendingDestroy();

    void BeginPlay();
    void EndPlay();

    // Game worlds tick every actor after BeginPlay. Editor worlds only tick actors
    // whose ShouldTickIfViewportsOnly() returns true (Unreal's LEVELTICK_ViewportsOnly).
    void Tick(float DeltaTime);

    // Render-side mirror of this world (one proxy per primitive component).
    FScene* GetScene() const { return Scene; }

    // Pushes this frame's component changes to the scene (Unreal's SendAllEndOfFrameUpdates).
    // Call on the game thread after all gameplay/editor changes and before rendering.
    void SendAllEndOfFrameUpdates();

    EWorldType GetWorldType() const { return WorldType; }
    void SetWorldType(EWorldType InWorldType) { WorldType = InWorldType; }
    bool IsEditorWorld() const { return WorldType == EWorldType::Editor; }

private:
    FName MakeUniqueActorName(FName RequestedName, const UClass* ActorClass) const;

    ULevel* PersistentLevel = nullptr;
    FScene* Scene = nullptr;
    TArray<AActor*> PendingDestroyActors;

    EWorldType WorldType = EWorldType::None;
    bool bHasBegunPlay = false;
};
