#pragma once
#include "Core/Math/Transform.h"
#include "UObject/Class.h"
#include "UObject/Object.h"

class USceneComponent : public UObject
{
    DECLARE_CLASS(USceneComponent, UObject);
public:
    FTransform GetTransform() const { return Transform; }
    void SetTransform(FTransform InTransform){ Transform = InTransform; }
private:
    FTransform Transform;
};
