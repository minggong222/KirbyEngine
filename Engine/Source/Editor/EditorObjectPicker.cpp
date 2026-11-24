#include "EditorObjectPicker.h"

#include "EditorObjectContext.h"

#include "Components/PrimitiveComponent.h"
#include "Core/Math/MathUtility.h"
#include "World/World.h"
#include "World/Level.h"
#include "Actors/Actor.h"
#include "Components/ActorComponent.h"

#include <cmath>
#include <limits>

#include "Components/StaticMeshComponent.h"
#include "Resources/StaticMesh.h"
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

            ULevel* Level = World != nullptr ? World->GetPersistentLevel() : nullptr;

            if (Level == nullptr)
            {
                return nullptr;
            }

            for (AActor* Actor : Level->GetActors())
            {
                if (Actor == nullptr)
                {
                    continue;
                }

                for (UActorComponent* ActorComponent : Actor->GetComponents())
                {
                    UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(ActorComponent);

                    if (Component == nullptr)
                    {
                        continue;
                    }

                    const UStaticMesh* Mesh = Component->GetStaticMesh();

                    if (Mesh == nullptr)
                    {
                        continue;
                    }

                    const FStaticMeshRenderData& MeshData = Mesh->GetRenderData();

                    const TArray<FVector>& Positions = MeshData.Positions;
                    const TArray<uint32>& Indices = MeshData.Indices;
                    
                    if (Positions.Num() < 3 || Indices.Num() < 3 || Indices.Num() % 3 != 0)
                    {
                        continue;
                    }

                    if (!MeshData.LocalBounds.IsValid())
                    {
                        continue;
                    }

                    const FMatrix WorldMatrix = Component->GetWorldMatrix();

                    const float Determinant = WorldMatrix.Determinant();

                    if (!std::isfinite(Determinant) || std::fabs(Determinant) <= THRESH_MATRIX_INVERSE_DETERMINANT)
                    {
                        continue;
                    }

                    const FMatrix InverseWorld = WorldMatrix.Invert();

                    const FVector LocalOrigin = FVector::Transform(WorldRay.position, InverseWorld);

                    const FVector LocalDirection = FVector::TransformNormal(WorldRay.direction, InverseWorld);

                    const float LocalDirectionLengthSquared = LocalDirection.LengthSquared();

                    if (!std::isfinite(LocalDirectionLengthSquared) || LocalDirectionLengthSquared <= EPSILON_1E_12)
                    {
                        continue;
                    }

                    const FRay LocalRay(LocalOrigin, LocalDirection);
                    
                    float BoundsHitT = 0.0f;

                    if (!MeshData.LocalBounds.Intersect(LocalRay, BoundsHitT))
                    {
                        continue;
                    }

                    if (BoundsHitT > ClosestT)
                    {
                        continue;
                    }
                    
                    bool bHasTriangleHit = false;

                    float ObjectHitT = (std::numeric_limits<float>::max)();

                    for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
                    {
                        const uint32 I0 = Indices[Index];
                        const uint32 I1 = Indices[Index + 1];
                        const uint32 I2 = Indices[Index + 2];

                        if (I0 >= static_cast<uint32>(Positions.Num()) || I1 >= static_cast<uint32>(Positions.Num()) || I2 >= static_cast<uint32>(Positions.Num()))
                        {
                            continue;
                        }

                        const FVector& V0 = Positions[static_cast<int32>(I0)];
                        const FVector& V1 = Positions[static_cast<int32>(I1)];
                        const FVector& V2 = Positions[static_cast<int32>(I2)];

                        float TriangleHitT = 0.0f;

                        if (!LocalRay.Intersects(V0, V1, V2, TriangleHitT))
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
