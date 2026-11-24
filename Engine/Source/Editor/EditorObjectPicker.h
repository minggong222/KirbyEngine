#pragma once
#include "Core/STL.h"
#include "Core/Math/Math.h"

struct FEditorObjectContext;
class UObject;
class UWorld;
class UStaticMeshComponent;

namespace EditorPicking
{
    UStaticMeshComponent* PickComponent(UWorld* World, float U, float V, uint32 ViewportWidth, uint32 ViewportHeight, const FMatrix& View, const FMatrix& Projection);
}