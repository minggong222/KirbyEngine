#pragma once
#include "Core/STL.h"

class UObject;
class FEditorUndoManager;
class FEditorTransformGizmo;

struct FEditorObjectContext
{
    TArray<UObject*> Objects;
    UObject* SelectedObject = nullptr;

    FEditorUndoManager* UndoManager = nullptr;
    FEditorTransformGizmo* TransformGizmo = nullptr;
};