#pragma once

#include "Core/STL.h"

#include <unordered_map>

class UWorld;
class UPrimitiveComponent;
class FPrimitiveSceneProxy;

// Renderer-side mirror of a world (Unreal's FScene): one proxy per primitive component.
// Owned by UWorld. The game thread updates it once per frame, after gameplay and editor
// changes and before rendering; the rendering thread then reads only the proxies.
class FScene
{
public:
    FScene() = default;
    ~FScene();

    FScene(const FScene&) = delete;
    FScene& operator=(const FScene&) = delete;

    // Game thread (Unreal's SendAllEndOfFrameUpdates): creates proxies for new
    // primitives, refreshes existing ones and removes proxies whose component is gone.
    void UpdatePrimitives(const UWorld& World);

    // Deletes every proxy.
    void Clear();

    const TArray<FPrimitiveSceneProxy*>& GetPrimitives() const { return Primitives; }

private:
    struct FProxyEntry
    {
        FPrimitiveSceneProxy* Proxy = nullptr;
        uint32 ComponentSerialNumber = 0;
        uint32 LastSeenFrame = 0;
    };

    std::unordered_map<const UPrimitiveComponent*, FProxyEntry> ProxyMap;
    TArray<FPrimitiveSceneProxy*> Primitives;
    uint32 UpdateFrame = 0;
};
