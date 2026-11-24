#include "StaticMeshComponent.h"

#include "Resources/StaticMesh.h"

FBox UStaticMeshComponent::CalcBounds() const
{
    FBox WorldBounds;

    if (StaticMesh == nullptr)
    {
        return WorldBounds;
    }

    const FStaticMeshRenderData& RenderData = StaticMesh->GetRenderData();

    if (!RenderData.LocalBounds.IsValid())
    {
        return WorldBounds;
    }

    const FVector& Min = RenderData.LocalBounds.GetMin();
    const FVector& Max = RenderData.LocalBounds.GetMax();
    const FMatrix WorldMatrix = GetWorldMatrix();

    for (int32 X = 0; X < 2; ++X)
    {
        for (int32 Y = 0; Y < 2; ++Y)
        {
            for (int32 Z = 0; Z < 2; ++Z)
            {
                const FVector LocalPosition(
                    X == 0 ? Min.x : Max.x,
                    Y == 0 ? Min.y : Max.y,
                    Z == 0 ? Min.z : Max.z
                );

                const FVector WorldPosition(
                    LocalPosition.x * WorldMatrix.M[0][0] + LocalPosition.y * WorldMatrix.M[1][0] + LocalPosition.z * WorldMatrix.M[2][0] + WorldMatrix.M[3][0],
                    LocalPosition.x * WorldMatrix.M[0][1] + LocalPosition.y * WorldMatrix.M[1][1] + LocalPosition.z * WorldMatrix.M[2][1] + WorldMatrix.M[3][1],
                    LocalPosition.x * WorldMatrix.M[0][2] + LocalPosition.y * WorldMatrix.M[1][2] + LocalPosition.z * WorldMatrix.M[2][2] + WorldMatrix.M[3][2]
                );

                WorldBounds.Add(WorldPosition);
            }
        }
    }

    return WorldBounds;
}