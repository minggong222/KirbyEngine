#pragma once
#include "Core/STL.h"

class UObject;

struct FEditorObjectContext
{
    TArray<UObject*> Objects;
    UObject* SelectedObject = nullptr;
};
