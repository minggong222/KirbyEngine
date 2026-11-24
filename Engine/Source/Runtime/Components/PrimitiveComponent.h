#pragma once
#include "SceneComponent.h"
#include "Core/Math/Box.h"
#include "Resources/StaticMeshData.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.generated.h"

UCLASS()
class UPrimitiveComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    virtual FBox CalcBounds() const { return FBox(); }
};