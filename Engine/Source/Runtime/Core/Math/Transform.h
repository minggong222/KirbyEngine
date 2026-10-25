#pragma once
#include "Math.h"
#include "Rotator.h"

class FTransform
{
public:
    FTransform() = default;
    FTransform(FVector InL, FVector InR, FVector InS = FVector::Zero)
        : Location(InL), Rotation(InR), Scale3D(InS){}
    
    FMatrix GetLocalMatrix() const;

    FVector GetForward() const;
    FVector GetUp() const;
    FVector GetRight() const;
    FQuat GetOrientation() const;
    
    FVector Location = FVector::Zero;
    FRotator Rotation = FVector::Zero;
    FVector Scale3D = FVector(1.0f, 1.0f, 1.0f);
};
