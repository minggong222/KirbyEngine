#include "Rotator.h"

#include "MathUtility.h"

FRotator::FRotator()
{
    Pitch = 0.0f;
    Yaw = 0.0f;
    Roll = 0.0f;
}

FRotator::FRotator(float P, float Y, float R)
{
    Pitch = P;
    Yaw = Y;
    Roll = R;
}

FRotator::FRotator(FVector V)
{
    Pitch = V.x;
    Yaw = V.y;
    Roll = V.z;
}

FQuat FRotator::Quaternion() const
{
    float PRad = Pitch * DEGREES_TO_RADIANS;
    float YRad = Yaw * DEGREES_TO_RADIANS;
    float RRad = Roll * DEGREES_TO_RADIANS;

    return FQuat::CreateFromYawPitchRoll(RRad, PRad, YRad);
}

FRotator operator+(FRotator Rot, const FVector& Vec)
{
    Rot.Roll += Vec.x;
    Rot.Pitch += Vec.y;
    Rot.Yaw += Vec.z;
    return Rot;
}

FRotator FRotator::Identitiy = FRotator(0, 0, 0);
