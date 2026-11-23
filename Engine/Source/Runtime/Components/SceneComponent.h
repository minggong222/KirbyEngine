
#pragma once
#include "Core/Math/Transform.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.generated.h"

UCLASS()
class USceneComponent : public UObject
{
    GENERATED_BODY()
public:
    FTransform GetTransform() const { return Transform; }
    void SetTransform(FTransform InTransform){ Transform = InTransform; }
private:
    FTransform Transform;
};
