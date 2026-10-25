#include "Rotator.h"

#include <algorithm>

#include "MathUtility.h"

FRotator::FRotator() : Pitch(0.0f), Yaw(0.0f), Roll(0.0f) {}

FRotator::FRotator(float P, float Y, float R) : Pitch(P), Yaw(Y), Roll(R) {}

FRotator::FRotator(FVector V) : Pitch(V.x), Yaw(V.y), Roll(V.z) {}

// Quaternion -> Euler
FRotator::FRotator(const FQuat& InQuat)
{
    FQuat NormalizedQuat = InQuat;
    NormalizedQuat.Normalize();
    
    const FMatrix M = NormalizedQuat.ToMatrix();
    
    const float SinPitch = std::clamp(M.m[0][2], -1.0f, 1.0f);
    
    const float PitchRadians = std::asin(SinPitch);
    const float CosPitch = std::cos(PitchRadians);
    
    float YawRadians;
    float RollRadians;
    
    if (std::fabs(CosPitch) > EPSILON_1E_6)
    {
        YawRadians = std::atan2(M.m[0][1], M.m[0][0]);
        RollRadians = std::atan2(M.m[1][2], M.m[2][2]);
    }
    else
    {
        YawRadians = std::atan2(-M.m[1][0], M.m[1][1]);
        RollRadians = 0.0f;
    }
    
    Pitch = PitchRadians * RADIANS_TO_DEGREES;
    Yaw = YawRadians * RADIANS_TO_DEGREES;
    Roll = RollRadians * RADIANS_TO_DEGREES;
}

// Euler -> Quaternion
FQuat FRotator::Quaternion() const
{
    float PRad = Pitch * DEGREES_TO_RADIANS;
    float YRad = Yaw * DEGREES_TO_RADIANS;
    float RRad = Roll * DEGREES_TO_RADIANS;

    const FMatrix PitchMatrix = FMatrix::CreateRotationY(-PRad);
    const FMatrix YawMatrix = FMatrix::CreateRotationZ(YRad);
    const FMatrix RollMatrix = FMatrix::CreateRotationX(-RRad);
    const FMatrix RotationMatrix = RollMatrix * PitchMatrix * YawMatrix;
    
    return FQuat::CreateFromRotationMatrix(RotationMatrix);
}

FRotator operator+(FRotator Rot, const FVector& Vec)
{
    Rot.Pitch += Vec.x;
    Rot.Yaw += Vec.y;
    Rot.Roll += Vec.z;
    return Rot;
}

FRotator FRotator::Identitiy = FRotator(0, 0, 0);
