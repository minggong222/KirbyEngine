#pragma once
#include "Math.h"

// Degree but Quat: Radians
class FRotator
{
public:
    FRotator();
    FRotator(float P, float Y, float R);
    FRotator(FVector V);
    explicit FRotator(const FQuat& InQuat);
    
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
    
    float Pitch = 0.0f;
    float Yaw = 0.0f;
    float Roll = 0.0f;
};

FRotator operator+(FRotator Rot, const FVector& Vec);



