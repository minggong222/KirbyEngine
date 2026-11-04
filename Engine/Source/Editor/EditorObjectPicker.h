#pragma once
#include "Core/STL.h"
#include "Core/Math/Math.h"

struct FEditorObjectContext;
class UObject;

namespace EditorPicking
{
    UObject* PickObject(const FEditorObjectContext& Context, float U, float V, uint32 ViewportWidth, uint32 ViewportHeight, const FMatrix& View, const FMatrix& Projection);
}