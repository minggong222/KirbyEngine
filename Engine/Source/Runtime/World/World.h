#pragma once

#include "Actors/Actor.h"
#include "World/Level.h"
#include "UObject/ObjectMacros.h"
#include "World/World.generated.h"

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

        Actor->SetActorTransform(InTransform);

        if (bHasBegunPlay)
        {
            Actor->BeginPlay();
        }

        return Actor;
    }

    bool DestroyActor(AActor* Actor);

    void BeginPlay();
    void EndPlay();
    void Tick(float DeltaTime);

private:
    FName MakeUniqueActorName(FName RequestedName, const UClass* ActorClass) const;

    ULevel* PersistentLevel = nullptr;

    bool bHasBegunPlay = false;
};
