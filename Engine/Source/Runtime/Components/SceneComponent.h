#pragma once
#include "Core/Math/Transform.h"
#include "UObject/Object.h"

class USceneComponent : public UObject
{
public:
    FTransform GetTransform() const { return Transform; }
    void SetTransform(FTransform InTransform){ Transform = InTransform; }
private:
    FTransform Transform;
};
