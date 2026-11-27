#pragma once

#include "Core/STL.h"
#include "UObject/WeakObjectPtr.h"

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

    // Weak so the selection clears itself when the selected object is destroyed or deleted.
    TWeakObjectPtr<AActor> SelectedActor;
    TWeakObjectPtr<UActorComponent> SelectedComponent;

    FEditorUndoManager* UndoManager = nullptr;
    FEditorTransformGizmo* TransformGizmo = nullptr;

    UObject* GetSelectedObject() const;
    USceneComponent* GetSelectedSceneComponent() const;

    void SelectActor(AActor* Actor);
    void SelectComponent(UActorComponent* Component);
    void ClearSelection();
};