#include "Actors/Actor.h"

#include "World/Level.h"
#include "World/World.h"

AActor::~AActor()
{
    EndPlay();

    while (!Components.IsEmpty())
    {
        UActorComponent* Component = Components.Pop();

        if (Component == nullptr)
        {
            continue;
        }

        if (Component == RootComponent)
        {
            RootComponent = nullptr;
        }

        Component->OnComponentDestroyed();
        delete Component;
    }

    RootComponent = nullptr;
}

UWorld* AActor::GetWorld() const
{
    ULevel* Level = GetLevel();

    return Level != nullptr ? Level->GetWorld() : nullptr;
}

ULevel* AActor::GetLevel() const
{
    return Cast<ULevel>(GetOuter());
}

UActorComponent* AActor::FindComponentByName(FName InName) const
{
    for (UActorComponent* Component : Components)
    {
        if (Component != nullptr && Component->GetFName() == InName)
        {
            return Component;
        }
    }

    return nullptr;
}

bool AActor::SetRootComponent(USceneComponent* InRootComponent)
{
    if (InRootComponent != nullptr)
    {
        if (InRootComponent->GetOwner() != this || !Components.Contains(InRootComponent))
        {
            return false;
        }
    }

    if (RootComponent == InRootComponent)
    {
        return true;
    }

    USceneComponent* PreviousRoot = RootComponent;

    if (InRootComponent != nullptr)
    {
        InRootComponent->DetachFromComponent(true);
    }

    if (PreviousRoot != nullptr)
    {
        const TArray<USceneComponent*> PreviousChildren = PreviousRoot->GetAttachChildren();

        for (USceneComponent* Child : PreviousChildren)
        {
            if (Child == nullptr)
            {
                continue;
            }

            if (InRootComponent != nullptr)
            {
                Child->AttachToComponent(InRootComponent, true);
            }
            else
            {
                Child->DetachFromComponent(true);
            }
        }

        PreviousRoot->DetachFromComponent(true);
    }

    RootComponent = InRootComponent;

    return true;
}

FTransform AActor::GetActorTransform() const
{
    return RootComponent != nullptr ? RootComponent->GetWorldTransform() : FTransform();
}

bool AActor::SetActorTransform(const FTransform& InTransform)
{
    return RootComponent != nullptr && RootComponent->SetWorldTransform(InTransform);
}

bool AActor::DestroyComponent(UActorComponent* Component)
{
    if (Component == nullptr || !Components.Contains(Component))
    {
        return false;
    }

    if (Component == RootComponent)
    {
        USceneComponent* NewRoot = nullptr;

        for (UActorComponent* Other : Components)
        {
            if (Other != Component)
            {
                NewRoot = Cast<USceneComponent>(Other);

                if (NewRoot != nullptr)
                {
                    break;
                }
            }
        }

        if (!SetRootComponent(NewRoot))
        {
            return false;
        }
    }

    Component->OnComponentDestroyed();
    Components.RemoveSingle(Component);
    delete Component;

    return true;
}

void AActor::BeginPlay()
{
    if (bHasBegunPlay)
    {
        return;
    }

    bHasBegunPlay = true;

    const TArray<UActorComponent*> CurrentComponents = Components;

    for (UActorComponent* Component : CurrentComponents)
    {
        if (Component != nullptr && Component->IsActive() && Components.Contains(Component))
        {
            Component->BeginPlay();
        }
    }
}

void AActor::EndPlay()
{
    if (!bHasBegunPlay)
    {
        return;
    }

    bHasBegunPlay = false;

    for (int32 Index = Components.Num() - 1; Index >= 0; --Index)
    {
        UActorComponent* Component = Components[Index];

        if (Component != nullptr && Components.Contains(Component))
        {
            Component->EndPlay();
        }
    }
}

void AActor::Tick(float DeltaTime)
{
    if (!bHasBegunPlay)
    {
        return;
    }

    const TArray<UActorComponent*> CurrentComponents = Components;

    for (UActorComponent* Component : CurrentComponents)
    {
        if (Component != nullptr && Component->IsActive() && Components.Contains(Component))
        {
            Component->TickComponent(DeltaTime);
        }
    }
}
