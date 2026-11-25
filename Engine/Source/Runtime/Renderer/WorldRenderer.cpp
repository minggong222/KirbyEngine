#include "Renderer/WorldRenderer.h"

#include "Renderer/Renderer.h"

#include "World/World.h"
#include "World/Level.h"

#include "Actors/Actor.h"

#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Resources/StaticMesh.h"
#include "Resources/StaticMeshData.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"

#include <cmath>
#include <unordered_set>

void FWorldRenderer::Render(UWorld* World, URenderer& Renderer, const FMatrix& View, const FMatrix& Projection, UObject* SelectedObject, const FEngineShowFlags& ShowFlags) const
{
    if (World == nullptr || !Renderer.IsReady() || !ShowFlags.IsEnabled(EEngineShowFlag::Primitives))
    {
        return;
    }

    ULevel* Level = World->GetPersistentLevel();

    if (Level == nullptr)
    {
        return;
    }

    TArray<UStaticMeshComponent*> OutlineComponents;

    for (AActor* Actor : Level->GetActors())
    {
        if (Actor == nullptr)
        {
            continue;
        }

        const bool bActorSelected = SelectedObject == Actor;

        for (UActorComponent* ActorComponent : Actor->GetComponents())
        {
            UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(ActorComponent);

            if (Component == nullptr)
            {
                continue;
            }

            RenderStaticMesh(Component, Renderer, View, Projection, false);

            if (bActorSelected || SelectedObject == Component)
            {
                OutlineComponents.Add(Component);
            }
        }
    }

    for (UStaticMeshComponent* Component : OutlineComponents)
    {
        RenderStaticMesh(Component, Renderer, View, Projection, true);
    }
}

void FWorldRenderer::RenderStaticMesh(UStaticMeshComponent* Component, URenderer& Renderer, const FMatrix& View, const FMatrix& Projection, bool bRenderOutline) const
{
    if (Component == nullptr)
    {
        return;
    }

    UStaticMesh* Mesh = Component->GetStaticMesh();

    if (Mesh == nullptr)
    {
        return;
    }

    const FStaticMeshRenderData& RenderData = Mesh->GetRenderData();

    if (RenderData.VertexBuffer == nullptr || RenderData.IndexBuffer == nullptr || RenderData.IndexCount == 0)
    {
        return;
    }

    FMatrix WorldMatrix = Component->GetWorldMatrix();

    if (bRenderOutline)
    {
        const FMatrix OutlineScale = FMatrix::CreateScale(FVector(1.03f, 1.03f, 1.03f));
        WorldMatrix = OutlineScale * WorldMatrix;
    }

    for (const FStaticMeshSection& Section : RenderData.Sections)
    {
        if (Section.IndexCount == 0 || Section.FirstIndex > RenderData.IndexCount || Section.IndexCount > RenderData.IndexCount - Section.FirstIndex)
        {
            continue;
        }

        UMaterial* Material = bRenderOutline ? nullptr : Component->GetMaterial(static_cast<int32>(Section.MaterialIndex));
        UTexture2D* Texture = Material != nullptr ? Material->GetBaseColorTexture() : nullptr;

        ID3D11ShaderResourceView* TextureSRV = Texture != nullptr && Texture->IsValid() ? Texture->GetResource().ShaderResourceView : nullptr;

        const FVector4 MaterialColor = Material != nullptr ? Material->GetBaseColor() : FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        const FVector2D UVOffset = Material != nullptr ? Material->GetUVOffset() : FVector2D(0.0f, 0.0f);

        Renderer.UpdateConstant(WorldMatrix, View, Projection, bRenderOutline, TextureSRV != nullptr && !bRenderOutline, false, MaterialColor, UVOffset, RenderData.bHasVertexNormals);
        Renderer.RenderPrimitive(RenderData.VertexBuffer, RenderData.IndexBuffer, Section.IndexCount, bRenderOutline, bRenderOutline ? nullptr : TextureSRV, Section.FirstIndex);
    }
}

void FWorldRenderer::TickMaterials(UWorld* World, float DeltaTime) const
{
    if (World == nullptr || !std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
    {
        return;
    }

    ULevel* Level = World->GetPersistentLevel();

    if (Level == nullptr)
    {
        return;
    }

    std::unordered_set<UMaterial*> TickedMaterials;

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

            const TArray<UMaterial*> Materials = Component->GetMaterials();

            for (UMaterial* Material : Materials)
            {
                if (Material != nullptr && TickedMaterials.insert(Material).second)
                {
                    Material->TickUVScroll(DeltaTime);
                }
            }
        }
    }
}