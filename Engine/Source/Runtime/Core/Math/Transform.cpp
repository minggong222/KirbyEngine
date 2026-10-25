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
    const FMatrix M = GetOrientation().ToMatrix();
    
    return FVector(M.m[0][0], M.m[0][1], M.m[0][2]);
}

FVector FTransform::GetRight() const
{
    const FMatrix M = GetOrientation().ToMatrix();
    
    return FVector(M.m[1][0], M.m[1][1], M.m[1][2]);
}

FVector FTransform::GetUp() const
{
    const FMatrix M = GetOrientation().ToMatrix();
    
    return FVector(M.m[2][0], M.m[2][1], M.m[2][2]);
}

FQuat FTransform::GetOrientation() const
{
    return Rotation.Quaternion();
}
