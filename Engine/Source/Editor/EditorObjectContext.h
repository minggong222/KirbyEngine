#pragma once

#include "Core/STL.h"

class UWorld;
class AActor;
class UObject;
class UActorComponent;
class USceneComponent;
class FEditorUndoManager;
class FEditorTransformGizmo;

struct FEditorObjectContext
{
    UWorld* World = nullptr;

    AActor* SelectedActor = nullptr;
    UActorComponent* SelectedComponent = nullptr;

    FEditorUndoManager* UndoManager = nullptr;
    FEditorTransformGizmo* TransformGizmo = nullptr;

    UObject* GetSelectedObject() const;
    USceneComponent* GetSelectedSceneComponent() const;

    void SelectActor(AActor* Actor);
    void SelectComponent(UActorComponent* Component);
    void ClearSelection();
};