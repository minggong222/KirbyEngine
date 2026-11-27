#pragma once

#include "Core/Math/SimpleMath.h"

#include <cmath>

using FVector2D = DirectX::SimpleMath::Vector2;
using FVector = DirectX::SimpleMath::Vector3;
using FVector4 = DirectX::SimpleMath::Vector4;
using FQuat = DirectX::SimpleMath::Quaternion;
using FLinearColor = DirectX::SimpleMath::Color;
using FPlane = DirectX::SimpleMath::Plane;
using FRay = DirectX::SimpleMath::Ray;
using FIntRect = DirectX::SimpleMath::Rectangle;
using FViewport = DirectX::SimpleMath::Viewport;

using FSimpleMathMatrix = DirectX::SimpleMath::Matrix;

struct FMatrix
{
    union
    {
        float M[4][4];
        float m[4][4];
    };

    FMatrix() noexcept
        : M{
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f}
        }
    {
    }

    FMatrix(float M00, float M01, float M02, float M03,
            float M10, float M11, float M12, float M13,
            float M20, float M21, float M22, float M23,
            float M30, float M31, float M32, float M33) noexcept
        : M{
            {M00, M01, M02, M03},
            {M10, M11, M12, M13},
            {M20, M21, M22, M23},
            {M30, M31, M32, M33}
        }
    {
    }

    FMatrix(const FSimpleMathMatrix& InMatrix) noexcept
    {
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                M[Row][Column] = InMatrix.m[Row][Column];
            }
        }
    }

    FMatrix(const FMatrix&) noexcept = default;
    FMatrix& operator=(const FMatrix&) noexcept = default;

    FMatrix& operator=(const FSimpleMathMatrix& InMatrix) noexcept
    {
        for (int Row = 0; Row < 4; ++Row)
        {
            for (int Column = 0; Column < 4; ++Column)
            {
                M[Row][Column] = InMatrix.m[Row][Column];
            }
        }

        return *this;
    }

    FSimpleMathMatrix ToSimpleMath() const noexcept
    {
        return FSimpleMathMatrix(&M[0][0]);
    }

    operator FSimpleMathMatrix() const noexcept
    {
        return ToSimpleMath();
    }

    FMatrix operator*(const FMatrix& Other) const noexcept
    {
        return FMatrix(ToSimpleMath() * Other.ToSimpleMath());
    }

    FMatrix& operator*=(const FMatrix& Other) noexcept
    {
        *this = *this * Other;
        return *this;
    }

    FMatrix Transpose() const noexcept
    {
        return FMatrix(ToSimpleMath().Transpose());
    }

    FMatrix Invert() const noexcept
    {
        return FMatrix(ToSimpleMath().Invert());
    }

    float Determinant() const noexcept
    {
        return ToSimpleMath().Determinant();
    }

    bool Decompose(FVector& OutScale, FQuat& OutRotation, FVector& OutTranslation) const noexcept
    {
        return ToSimpleMath().Decompose(OutScale, OutRotation, OutTranslation);
    }

    FMatrix GetNormalMatrix() const noexcept
    {
        FMatrix Result = Invert().Transpose();

        Result.M[0][3] = 0.0f;
        Result.M[1][3] = 0.0f;
        Result.M[2][3] = 0.0f;

        Result.M[3][0] = 0.0f;
        Result.M[3][1] = 0.0f;
        Result.M[3][2] = 0.0f;
        Result.M[3][3] = 1.0f;

        return Result;
    }

    bool IsOrthogonal(float Tolerance = 1.0e-4f) const noexcept
    {
        float LengthSquared[3] = {};

        for (int Row = 0; Row < 3; ++Row)
        {
            LengthSquared[Row] = M[Row][0] * M[Row][0] + M[Row][1] * M[Row][1] + M[Row][2] * M[Row][2];

            if (LengthSquared[Row] <= Tolerance)
            {
                return false;
            }
        }

        for (int RowA = 0; RowA < 3; ++RowA)
        {
            for (int RowB = RowA + 1; RowB < 3; ++RowB)
            {
                const float Dot = M[RowA][0] * M[RowB][0] + M[RowA][1] * M[RowB][1] + M[RowA][2] * M[RowB][2];
                const float Limit = Tolerance * std::sqrt(LengthSquared[RowA] * LengthSquared[RowB]);

                if (std::fabs(Dot) > Limit)
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool IsOrthonormal(float Tolerance = 1.0e-4f) const noexcept
    {
        for (int RowA = 0; RowA < 3; ++RowA)
        {
            for (int RowB = 0; RowB < 3; ++RowB)
            {
                const float Dot = M[RowA][0] * M[RowB][0] + M[RowA][1] * M[RowB][1] + M[RowA][2] * M[RowB][2];
                const float Expected = RowA == RowB ? 1.0f : 0.0f;

                if (std::fabs(Dot - Expected) > Tolerance)
                {
                    return false;
                }
            }
        }

        return true;
    }

    static FMatrix CreateTranslation(const FVector& Translation)
    {
        return FMatrix(FSimpleMathMatrix::CreateTranslation(Translation));
    }

    static FMatrix CreateScale(const FVector& Scale)
    {
        return FMatrix(FSimpleMathMatrix::CreateScale(Scale));
    }

    static FMatrix CreateRotationX(float Radians)
    {
        return FMatrix(FSimpleMathMatrix::CreateRotationX(Radians));
    }

    static FMatrix CreateRotationY(float Radians)
    {
        return FMatrix(FSimpleMathMatrix::CreateRotationY(Radians));
    }

    static FMatrix CreateRotationZ(float Radians)
    {
        return FMatrix(FSimpleMathMatrix::CreateRotationZ(Radians));
    }

    static FMatrix CreateFromAxisAngle(const FVector& Axis, float Angle)
    {
        return FMatrix(FSimpleMathMatrix::CreateFromAxisAngle(Axis, Angle));
    }

    static FMatrix CreateFromQuaternion(const FQuat& Rotation)
    {
        return FMatrix(FSimpleMathMatrix::CreateFromQuaternion(Rotation));
    }

    static FMatrix CreateLookAt(const FVector& Position, const FVector& Target, const FVector& Up)
    {
        return FMatrix(FSimpleMathMatrix::CreateLookAt(Position, Target, Up));
    }

    static FMatrix CreateOrthographic(float Width, float Height, float NearPlane, float FarPlane)
    {
        return FMatrix(FSimpleMathMatrix::CreateOrthographic(Width, Height, NearPlane, FarPlane));
    }

    static FMatrix CreatePerspectiveFieldOfView(float Fov, float AspectRatio, float NearPlane, float FarPlane)
    {
        return FMatrix(FSimpleMathMatrix::CreatePerspectiveFieldOfView(Fov, AspectRatio, NearPlane, FarPlane));
    }

    static const FMatrix Identity;
};

inline const FMatrix FMatrix::Identity = FMatrix();

namespace EngineAxis
{
    inline const FVector Forward = FVector(1.0f, 0.0f, 0.0f);
    inline const FVector Right = FVector(0.0f, 1.0f, 0.0f);
    inline const FVector Up = FVector(0.0f, 0.0f, 1.0f);
    inline const FVector Backward = FVector(-1.0f, 0.0f, 0.0f);
    inline const FVector Left = FVector(0.0f, -1.0f, 0.0f);
    inline const FVector Down = FVector(0.0f, 0.0f, -1.0f);
}