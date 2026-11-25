#include "StaticMeshComponent.h"

#include "Resources/StaticMesh.h"
#include "Resources/StaticMeshData.h"

#include <algorithm>

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* InStaticMesh)
{
    if (StaticMesh == InStaticMesh)
    {
        return;
    }

    StaticMesh = InStaticMesh;
    Materials.Reset();
    MaterialOverrideMask.Reset();
}

void UStaticMeshComponent::SetMaterial(int32 MaterialIndex, UMaterial* InMaterial)
{
    if (MaterialIndex < 0 || MaterialIndex >= 4096)
    {
        return;
    }

    const int32 RequiredCount = MaterialIndex + 1;

    if (Materials.Num() < RequiredCount)
    {
        Materials.SetNum(RequiredCount);
    }

    if (MaterialOverrideMask.Num() < RequiredCount)
    {
        MaterialOverrideMask.SetNum(RequiredCount);
    }

    Materials[MaterialIndex] = InMaterial;
    MaterialOverrideMask[MaterialIndex] = 1;
}

UMaterial* UStaticMeshComponent::GetMaterial(int32 MaterialIndex) const
{
    if (MaterialIndex < 0 || MaterialIndex >= 4096)
    {
        return nullptr;
    }

    if (MaterialIndex < MaterialOverrideMask.Num() && MaterialOverrideMask[MaterialIndex] != 0)
    {
        return MaterialIndex < Materials.Num() ? Materials[MaterialIndex] : nullptr;
    }

    if (StaticMesh == nullptr)
    {
        return nullptr;
    }

    const FStaticMeshRenderData& RenderData = StaticMesh->GetRenderData();

    return MaterialIndex < RenderData.Materials.Num() ? RenderData.Materials[MaterialIndex] : nullptr;
}

int32 UStaticMeshComponent::GetMaterialCount() const
{
    int32 Count = (std::max)(Materials.Num(), MaterialOverrideMask.Num());

    if (StaticMesh == nullptr)
    {
        return Count;
    }

    const FStaticMeshRenderData& RenderData = StaticMesh->GetRenderData();
    Count = (std::max)(Count, RenderData.Materials.Num());

    for (const FStaticMeshSection& Section : RenderData.Sections)
    {
        if (Section.MaterialIndex < 4096)
        {
            Count = (std::max)(Count, static_cast<int32>(Section.MaterialIndex + 1));
        }
    }

    return Count;
}

TArray<UMaterial*> UStaticMeshComponent::GetMaterials() const
{
    TArray<UMaterial*> Result;
    const int32 Count = GetMaterialCount();

    Result.SetNum(Count);

    for (int32 Index = 0; Index < Count; ++Index)
    {
        Result[Index] = GetMaterial(Index);
    }

    return Result;
}

TArray<UMaterial*> UStaticMeshComponent::GetMaterialOverrides() const
{
    TArray<UMaterial*> Result;
    Result.SetNum(GetMaterialCount());

    for (int32 Index = 0; Index < Result.Num(); ++Index)
    {
        if (Index < Materials.Num())
        {
            Result[Index] = Materials[Index];
        }
    }

    return Result;
}

TArray<uint8> UStaticMeshComponent::GetMaterialOverrideMask() const
{
    TArray<uint8> Result;
    Result.SetNum(GetMaterialCount());

    for (int32 Index = 0; Index < Result.Num(); ++Index)
    {
        if (Index < MaterialOverrideMask.Num())
        {
            Result[Index] = MaterialOverrideMask[Index] != 0 ? 1 : 0;
        }
    }

    return Result;
}

void UStaticMeshComponent::SetMaterials(const TArray<UMaterial*>& InMaterials)
{
    Materials = InMaterials;
    MaterialOverrideMask.SetNum(InMaterials.Num());

    for (int32 Index = 0; Index < MaterialOverrideMask.Num(); ++Index)
    {
        MaterialOverrideMask[Index] = 1;
    }
}

void UStaticMeshComponent::SetMaterialOverrides(const TArray<UMaterial*>& InMaterials, const TArray<uint8>& InOverrideMask)
{
    const int32 Count = (std::max)(InMaterials.Num(), InOverrideMask.Num());

    Materials = InMaterials;
    MaterialOverrideMask = InOverrideMask;

    Materials.SetNum(Count);
    MaterialOverrideMask.SetNum(Count);

    for (int32 Index = 0; Index < Count; ++Index)
    {
        MaterialOverrideMask[Index] = MaterialOverrideMask[Index] != 0 ? 1 : 0;
    }
}

bool UStaticMeshComponent::HasMaterialOverride(int32 MaterialIndex) const
{
    return MaterialIndex >= 0 && MaterialIndex < MaterialOverrideMask.Num() && MaterialOverrideMask[MaterialIndex] != 0;
}

void UStaticMeshComponent::ClearMaterialOverride(int32 MaterialIndex)
{
    if (MaterialIndex < 0 || MaterialIndex >= MaterialOverrideMask.Num())
    {
        return;
    }

    MaterialOverrideMask[MaterialIndex] = 0;

    if (MaterialIndex < Materials.Num())
    {
        Materials[MaterialIndex] = nullptr;
    }
}

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
                const FVector LocalPosition(X == 0 ? Min.x : Max.x, Y == 0 ? Min.y : Max.y, Z == 0 ? Min.z : Max.z);

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