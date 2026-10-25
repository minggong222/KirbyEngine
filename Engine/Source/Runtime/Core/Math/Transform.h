#pragma once
#include "Math.h"
#include "Rotator.h"

class FTransform
{
public:
    FTransform() = default;
    FTransform(FVector InL, FRotator InR, FVector InS = FVector::One)
        : Location(InL), Rotation(InR), Scale3D(InS){}
    
    FMatrix GetLocalMatrix() const;

    FVector GetForward() const;
    FVector GetUp() const;
    FVector GetRight() const;
    FQuat GetOrientation() const;
    
    FVector Location = FVector::Zero;
    FRotator Rotation = FRotator();
    FVector Scale3D = FVector::One;
};
