#pragma once

#include "Core/Math/Math.h"

class UStaticMeshComponent;
class UWorld;
class UObject;
class URenderer;

class FWorldRenderer
{
public:
    void Render(UWorld* World, URenderer& Renderer, const FMatrix& View, const FMatrix& Projection, UObject* SelectedObject = nullptr) const;

private:
    void RenderStaticMesh(UStaticMeshComponent* Component, URenderer& Renderer, const FMatrix& View, const FMatrix& Projection, bool bRenderOutline) const;
};
