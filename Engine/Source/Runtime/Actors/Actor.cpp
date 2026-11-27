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

UActorComponent* AActor::CreateComponent(const UClass* ComponentClass, FName InName)
{
    if (ComponentClass == nullptr || !ComponentClass->IsChildOf(UActorComponent::StaticClass()) || ComponentClass == UActorComponent::StaticClass())
    {
        return nullptr;
    }

    const FName BaseName = FName(ComponentClass->GetName());
    FName ComponentName = InName.IsNone() ? BaseName : InName;
    int32 InternalNumber = 2;

    while (FindComponentByName(ComponentName) != nullptr)
    {
        ComponentName = FName(BaseName, InternalNumber++);
    }

    UActorComponent* Component = Cast<UActorComponent>(NewObject(ComponentClass, this, ComponentName));

    if (Component == nullptr)
    {
        return nullptr;
    }

    Components.Add(Component);

    if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component); SceneComponent != nullptr && RootComponent == nullptr)
    {
        RootComponent = SceneComponent;
    }

    Component->OnComponentCreated();

    return Component;
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
    // Editor worlds never call BeginPlay; opted-in actors still tick there.
    if (!bHasBegunPlay && !ShouldTickIfViewportsOnly())
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
