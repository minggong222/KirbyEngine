#include "EditorObjectContext.h"

#include "Actors/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "UObject/Class.h"

UObject* FEditorObjectContext::GetSelectedObject() const
{
    if (SelectedComponent != nullptr)
    {
        return SelectedComponent;
    }

    return SelectedActor;
}

USceneComponent* FEditorObjectContext::GetSelectedSceneComponent() const
{
    if (SelectedComponent != nullptr)
    {
        return Cast<USceneComponent>(SelectedComponent);
    }

    return SelectedActor != nullptr ? SelectedActor->GetRootComponent() : nullptr;
}

void FEditorObjectContext::SelectActor(AActor* Actor)
{
    SelectedActor = Actor;
    SelectedComponent = nullptr;
}

void FEditorObjectContext::SelectComponent(UActorComponent* Component)
{
    SelectedComponent = Component;
    SelectedActor = Component != nullptr ? Component->GetOwner() : nullptr;
}

void FEditorObjectContext::ClearSelection()
{
    SelectedActor = nullptr;
    SelectedComponent = nullptr;
}