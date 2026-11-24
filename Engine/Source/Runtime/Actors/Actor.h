#pragma once

#include "Components/SceneComponent.h"
#include "UObject/ObjectMacros.h"
#include "Actors/Actor.generated.h"

class UWorld;
class ULevel;

UCLASS()
class AActor : public UObject
{
    GENERATED_BODY()

public:
    AActor() = default;
    virtual ~AActor() override;

    UWorld* GetWorld() const;
    ULevel* GetLevel() const;

    USceneComponent* GetRootComponent() const { return RootComponent; }
    const TArray<UActorComponent*>& GetComponents() const { return Components; }

    bool SetRootComponent(USceneComponent* InRootComponent);

    FTransform GetActorTransform() const;
    bool SetActorTransform(const FTransform& InTransform);

    bool DestroyComponent(UActorComponent* Component);

    UActorComponent* FindComponentByName(FName InName) const;

    template<typename TComponent>
    TComponent* FindComponentByClass() const
    {
        static_assert(std::is_base_of_v<UActorComponent, TComponent>, "TComponent must derive from UActorComponent");

        for (UActorComponent* Component : Components)
        {
            if (TComponent* TypedComponent = Cast<TComponent>(Component))
            {
                return TypedComponent;
            }
        }

        return nullptr;
    }

    UActorComponent* CreateComponent(const UClass* ComponentClass, FName InName = FName());
    template<typename TComponent>
    TComponent* CreateComponent(FName InName = FName())
    {
        static_assert(std::is_base_of_v<UActorComponent, TComponent>, "TComponent must derive from UActorComponent");
        return Cast<TComponent>(CreateComponent(TComponent::StaticClass(), InName));
    }

    virtual void BeginPlay();
    virtual void EndPlay();
    virtual void Tick(float DeltaTime);

private:
    TArray<UActorComponent*> Components;
    USceneComponent* RootComponent = nullptr;

    bool bHasBegunPlay = false;
};
