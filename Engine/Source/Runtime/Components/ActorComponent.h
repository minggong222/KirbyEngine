#pragma once

#include "UObject/Class.h"
#include "UObject/ObjectMacros.h"
#include "Components/ActorComponent.generated.h"

class AActor;

UCLASS()
class UActorComponent : public UObject
{
    GENERATED_BODY()

public:
    UActorComponent() = default;
    virtual ~UActorComponent() override = default;

    AActor* GetOwner() const;

    bool IsActive() const { return bIsActive; }
    void SetActive(bool bNewActive) { bIsActive = bNewActive; }

    virtual void OnComponentCreated() {}
    virtual void OnComponentDestroyed() {}

    virtual void BeginPlay() {}
    virtual void EndPlay() {}
    virtual void TickComponent(float DeltaTime) {}

private:
    bool bIsActive = true;
};
