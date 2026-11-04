#include "EditorObjectPicker.h"

#include "EditorObjectContext.h"

#include "Components/PrimitiveComponent.h"
#include "Core/Math/MathUtility.h"

#include <cmath>
#include <limits>

namespace EditorPicking
{
    namespace 
    {
        bool CreateWorldRay(float U, float V, uint32 Width, uint32 Height, const FMatrix& View, const FMatrix& Projection, FRay& OutRay)
        {
            if (Width == 0 || Height == 0)
            {
                return false;
            }
            if (!std::isfinite(U) || !std::isfinite(V) || U < 0.0f || U > 1.0f || V < 0.0f || V > 1.0f)
            {
                return false;
            }
            
            const FViewport Viewport(0.0f, 0.0f,  static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f);
            const float PixelX = U * static_cast<float>(Width);
            const float PixelY = V * static_cast<float>(Height);
            const FVector NearPoint = Viewport.Unproject(FVector(PixelX,PixelY,0.0f), Projection, View, FMatrix::Identity);
            const FVector FarPoint = Viewport.Unproject(FVector(PixelX,PixelY,1.0f), Projection, View, FMatrix::Identity);
            FVector Direction = FarPoint - NearPoint;
            const float LengthSquared = Direction.LengthSquared();
            if (!std::isfinite(LengthSquared) || LengthSquared <= EPSILON_1E_12)
            {
                return false;
            }
            Direction.Normalize();
            OutRay = FRay(NearPoint, Direction);
            return true;
        }
        UObject* FindNearestObject(const FEditorObjectContext& Context, const FRay& WorldRay)
        {
            UObject* ClosestObject = nullptr;

            float ClosestT = (std::numeric_limits<float>::max)();

            for (UObject* Object : Context.Objects)
            {
                if (Object == nullptr)
                {
                    continue;
                }
                UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Object);

                if (Primitive == nullptr)
                {
                    continue;
                }

                const FStaticMeshData* MeshData = Primitive->GetStaticMeshData();

                if (MeshData == nullptr)
                {
                    continue;
                }

                const TArray<FVector>& Positions = MeshData->Positions;

                if (Positions.Num() < 3 || Positions.Num() % 3 != 0)
                {
                    continue;
                }

                if (!MeshData->LocalBounds.IsValid())
                {
                    continue;
                }

                const FMatrix WorldMatrix = Primitive->GetTransform().GetLocalMatrix();

                const float Determinant = WorldMatrix.Determinant();

                if (!std::isfinite(Determinant) || std::fabs(Determinant) <= THRESH_MATRIX_INVERSE_DETERMINANT)
                {
                    continue;
                }

                const FMatrix InverseWorld = WorldMatrix.Invert();

                const FVector LocalOrigin = FVector::Transform(WorldRay.position, InverseWorld);

                const FVector LocalDirection = FVector::TransformNormal(WorldRay.direction, InverseWorld);

                const float LocalDirectionLengthSquared  = LocalDirection.LengthSquared();

                if (!std::isfinite(LocalDirectionLengthSquared ) || LocalDirectionLengthSquared  <= EPSILON_1E_12)
                {
                    continue;
                }
                
                const float LocalDirectionLength = std::sqrt(LocalDirectionLengthSquared);
                if (!std::isfinite(LocalDirectionLength) || LocalDirectionLength <= 0.0f)
                {
                    continue;
                }

                const FRay LocalBoundsRay(LocalOrigin, LocalDirection);
                
                FVector NormalizedLocalDirection = LocalDirection;
                NormalizedLocalDirection.Normalize();

                const FRay LocalTriangleRay(LocalOrigin, NormalizedLocalDirection);
                
                float BoundsHitT = 0.0f;

                if (!MeshData->LocalBounds.Intersect(LocalBoundsRay, BoundsHitT))
                {
                    continue;
                }

                if (BoundsHitT > ClosestT)
                {
                    continue;
                }
                
                bool bHasTriangleHit = false;

                float ObjectHitT = (std::numeric_limits<float>::max)();

                for (int32 Index = 0; Index + 2 < Positions.Num(); Index += 3)
                {
                    const FVector& V0 = Positions[Index];
                    const FVector& V1 = Positions[Index + 1];
                    const FVector& V2 = Positions[Index + 2];

                    float TriangleHitT = 0.0f;

                    if (!LocalTriangleRay.Intersects(V0, V1, V2, TriangleHitT))
                    {
                        continue;
                    }

                    if (!std::isfinite(TriangleHitT) || TriangleHitT < 0.0f)
                    {
                        continue;
                    }

                    if (TriangleHitT < ObjectHitT)
                    {
                        ObjectHitT = TriangleHitT;
                        bHasTriangleHit = true;
                    }
                }

                if (bHasTriangleHit && ObjectHitT < ClosestT)
                {
                    ClosestT = ObjectHitT;
                    ClosestObject = Object;
                }
            }

            return ClosestObject;
        }
    }
    UObject* PickObject(const FEditorObjectContext& Context, float U, float V, uint32 ViewportWidth,
        uint32 ViewportHeight, const FMatrix& View, const FMatrix& Projection)
    {
        FRay WorldRay;
        if (!CreateWorldRay(U, V, ViewportWidth, ViewportHeight, View, Projection, WorldRay))
        {
            return nullptr;
        }
        return FindNearestObject(Context, WorldRay);
    }
}
