#include "EditorObjectPicker.h"

#include "EditorObjectContext.h"

#include "Components/PrimitiveComponent.h"
#include "Core/Math/MathUtility.h"

#include <cmath>
#include <limits>

namespace EditorPicking
{
    UObject* FindNearestObject(FEditorObjectContext& Context, const FRay& WorldRay)
    {
        UObject* ClosestObject = nullptr;
        float ClosestT = (std::numeric_limits<float>::max)();
        const float DirectionLengthSquared = WorldRay.direction.LengthSquared();
        if (!std::isfinite(DirectionLengthSquared) || DirectionLengthSquared <= EPSILON_1E_12)
        {
            return nullptr;
        }
        for (const FEditorObjectEntry& Entry : Context.Objects)
        {
            UObject* Object = Entry.Object;
            if (Object == nullptr)
            {
                continue;
            }
            UPrimitiveComponent* Primitive  = Cast<UPrimitiveComponent>(Object);
            if (Primitive == nullptr)
            {
                continue;
            }
            
            const FBox& LocalBounds = Primitive->GetLocalBounds();
            
            if (!LocalBounds.IsValid())
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
            const float LocalDirectionLengthSquared = LocalDirection.LengthSquared();
            
            if (!std::isfinite(LocalDirectionLengthSquared) || LocalDirectionLengthSquared <= 0.0f)
            {
                continue;
            }
            
            const FRay LocalRay(LocalOrigin, LocalDirection);
            float HitT = 0.0f;
            if (!LocalBounds.Intersect(LocalRay, HitT))
            {
                continue;
            }
            
            if (HitT < ClosestT)
            {
                ClosestT = HitT;
                ClosestObject = Object;
            }
        }
        return ClosestObject;
    }
}