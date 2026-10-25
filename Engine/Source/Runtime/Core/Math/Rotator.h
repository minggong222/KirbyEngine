#pragma once
#include "Math.h"

// Degree but Quat: Radians
class FRotator
{
public:
    FRotator();
    FRotator(float P, float Y, float R);
    FRotator(FVector V);

    static FRotator Identitiy;

    FQuat Quaternion() const;

    FRotator operator+(const FRotator& Other)
    {
        return FRotator(Pitch + Other.Pitch, Yaw + Other.Yaw, Roll + Other.Roll);
    }

    FRotator operator*(const float Scalar)
    {
        return FRotator(Pitch * Scalar, Yaw * Scalar, Roll * Scalar);
    }
    
    float Pitch;
    float Yaw;
    float Roll;
};

FRotator operator+(FRotator Rot, const FVector& Vec);



