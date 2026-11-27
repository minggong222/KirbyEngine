#pragma once

#include "Core/STL.h"

// What a world is used for (Unreal's EWorldType).
enum class EWorldType : uint8
{
    None,
    Editor, // edited in the editor: gameplay does not run, only opted-in actors tick
    Game    // running game: BeginPlay/Tick for every actor
};
