#include "Renderer/Scene.h"

#include "Renderer/PrimitiveSceneProxy.h"

#include "World/World.h"
#include "World/Level.h"
#include "Actors/Actor.h"
#include "Components/PrimitiveComponent.h"

FScene::~FScene()
{
    Clear();
}

void FScene::Clear()
{
    for (auto& Pair : ProxyMap)
    {
        delete Pair.second.Proxy;
    }

    ProxyMap.clear();
    Primitives.Reset();
}

void FScene::UpdatePrimitives(const UWorld& World)
{
    ++UpdateFrame;

    if (const ULevel* Level = World.GetPersistentLevel())
    {
        for (AActor* Actor : Level->GetActors())
        {
            if (!IsValid(Actor))
            {
                continue;
            }

            for (UActorComponent* ActorComponent : Actor->GetComponents())
            {
                const UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(ActorComponent);

                if (!IsValid(Component))
                {
                    continue;
                }

                FProxyEntry& Entry = ProxyMap[Component];

                // A different object at a recycled address gets a fresh proxy.
                if (Entry.Proxy != nullptr && Entry.ComponentSerialNumber != Component->GetSerialNumber())
                {
                    delete Entry.Proxy;
                    Entry.Proxy = nullptr;
                }

                if (Entry.Proxy == nullptr)
                {
                    Entry.Proxy = Component->CreateSceneProxy();
                    Entry.ComponentSerialNumber = Component->GetSerialNumber();
                }
                else
                {
                    Entry.Proxy->UpdateFromComponent(*Component);
                }

                Entry.LastSeenFrame = UpdateFrame;
            }
        }
    }

    // Drop proxies of primitives that were destroyed or removed this frame, and rebuild the draw list.
    Primitives.Reset();

    for (auto It = ProxyMap.begin(); It != ProxyMap.end();)
    {
        if (It->second.LastSeenFrame != UpdateFrame || It->second.Proxy == nullptr)
        {
            delete It->second.Proxy;
            It = ProxyMap.erase(It);
            continue;
        }

        Primitives.Add(It->second.Proxy);
        ++It;
    }
}
