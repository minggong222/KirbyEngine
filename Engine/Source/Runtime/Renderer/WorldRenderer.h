#pragma once

#include "Core/Math/Math.h"
#include "Renderer/RenderSettings.h"

class UStaticMeshComponent;
class UWorld;
class UObject;
class FRHI;
class FScene;

class FWorldRenderer
{
public:
    // Rendering thread: draws every primitive proxy in the scene, then outlines the
    // proxies whose component or owning actor is SelectedObject (compared, never dereferenced).
    void Render(const FScene& Scene, FRHI& Renderer, const FMatrix& View, const FMatrix& Projection, const void* SelectedObject = nullptr, const FEngineShowFlags& ShowFlags = FEngineShowFlags()) const;

    void TickMaterials(UWorld* World, float DeltaTime) const;

};