#pragma once
#include "SceneComponent.h"
#include "Core/Math/Box.h"
#include "Resources/StaticMeshData.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.generated.h"

class FPrimitiveSceneProxy;

UCLASS()
class UPrimitiveComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    virtual FBox CalcBounds() const { return FBox(); }

    // Creates the render-side copy of this component (see FPrimitiveSceneProxy).
    // Called by FScene on the game thread. Components with nothing to draw return null.
    virtual FPrimitiveSceneProxy* CreateSceneProxy() const { return nullptr; }
};