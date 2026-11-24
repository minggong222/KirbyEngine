#pragma once

#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"
#include "World/Level.generated.h"

class AActor;
class UWorld;

UCLASS()
class ULevel : public UObject
{
    GENERATED_BODY()

public:
    ULevel() = default;
    virtual ~ULevel() override;

    UWorld* GetWorld() const;

    const TArray<AActor*>& GetActors() const { return Actors; }

    bool AddActor(AActor* Actor);
    AActor* FindActorByName(FName InName) const;

private:
    friend class UWorld;

    bool RemoveActor(AActor* Actor);

    TArray<AActor*> Actors;
};
