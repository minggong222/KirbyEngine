#pragma once
#include "Core/Math/Math.h"

struct FEditorObjectContext;
class UObject;

namespace EditorPicking
{
    UObject* FindNearestObject(FEditorObjectContext& Context, const FRay& WorldRay);
}