#include "Transform.h"

FMatrix FTransform::GetLocalMatrix() const
{
    FMatrix RotationMatrix = Rotation.Quaternion().ToMatrix();
    FMatrix WorldMatrix = FMatrix(
            RotationMatrix.m[0][0] * Scale3D.x, RotationMatrix.m[0][1] * Scale3D.x, RotationMatrix.m[0][2] * Scale3D.x, 0.0f,
            RotationMatrix.m[1][0] * Scale3D.y, RotationMatrix.m[1][1] * Scale3D.y, RotationMatrix.m[1][2] * Scale3D.y, 0.0f,
            RotationMatrix.m[2][0] * Scale3D.x, RotationMatrix.m[2][1] * Scale3D.x, RotationMatrix.m[2][2] * Scale3D.x, 0.0f,
            Location.x,                         Location.y,                         Location.z,                         1.0f    
            );
    return WorldMatrix;
}

FVector FTransform::GetForward() const
{
    return FVector::Forward;
}

FVector FTransform::GetUp() const
{
    return FVector::Up;
}

FVector FTransform::GetRight() const
{
    return FVector::Right;
}

FQuat FTransform::GetOrientation() const
{
    return FQuat::Identity;
}
