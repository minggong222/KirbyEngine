
#pragma once

#include "Core/STL.h"

enum class EViewMode : uint8
{
    Lit,
    Wireframe
};

struct FEngineShowFlags
{
    bool StaticMeshes = true;
    bool Grid = true;
    bool Axes = true;
    bool Bounds = false;
    bool Billboards = true;
    bool ActorNames = false;
};
