#pragma once
#include <d3d11.h>
#include "Core/STL.h"

class UObject;

struct FEditorObjectEntry
{
    UObject* Object = nullptr;
    ID3D11Buffer* VertexBuffer = nullptr;
    uint32 VertexCount = 0;
};

struct FEditorObjectContext
{
    TArray<FEditorObjectEntry> Objects;
    UObject* SelectedObject = nullptr;
};
