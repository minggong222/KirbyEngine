#include "Renderer/WorldRenderer.h"
#include "Renderer/Scene.h"
#include "Renderer/PrimitiveSceneProxy.h"

#include "RHI/RHI.h"

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

void FWorldRenderer::Render(const FScene& Scene, FRHI& Renderer, const FMatrix& View, const FMatrix& Projection, const void* SelectedObject, const FEngineShowFlags& ShowFlags) const
{
    if (!Renderer.IsReady() || !ShowFlags.IsEnabled(EEngineShowFlag::Primitives))
    {
        return;
    }

    TArray<const FPrimitiveSceneProxy*> OutlineProxies;

    for (const FPrimitiveSceneProxy* Proxy : Scene.GetPrimitives())
    {
        if (Proxy == nullptr)
        {
            continue;
        }

        Proxy->Draw(Renderer, View, Projection, false);

        if (SelectedObject != nullptr && (Proxy->GetPrimitiveId() == SelectedObject || Proxy->GetOwnerId() == SelectedObject))
        {
            OutlineProxies.Add(Proxy);
        }
    }

    for (const FPrimitiveSceneProxy* Proxy : OutlineProxies)
    {
        Proxy->Draw(Renderer, View, Projection, true);
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
