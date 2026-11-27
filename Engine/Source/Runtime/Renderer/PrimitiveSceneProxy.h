#pragma once

#include "Core/STL.h"
#include "Core/Math/Math.h"
#include "Core/Math/Box.h"
#include "RHI/RHIResources.h"

class FRHI;
class UPrimitiveComponent;
class UStaticMeshComponent;

// Render-side copy of a primitive component (Unreal's FPrimitiveSceneProxy).
//
// The game thread owns UObjects; the renderer only reads proxies. A proxy holds
// plain data copied from its component (transform, bounds, GPU buffers, material
// parameters), so drawing never dereferences a UObject. FScene refreshes proxies
// from their components once per frame on the game thread.
class FPrimitiveSceneProxy
{
public:
    FPrimitiveSceneProxy(const UPrimitiveComponent& Component);
    virtual ~FPrimitiveSceneProxy() = default;

    // Game thread: copy the component's current state into the proxy.
    virtual void UpdateFromComponent(const UPrimitiveComponent& Component);

    // Rendering thread: issue draw calls.
    virtual void Draw(FRHI& RHI, const FMatrix& View, const FMatrix& Projection, bool bRenderOutline) const = 0;

    // Identity of the component and its owning actor. Used only for comparisons
    // (selection highlighting), never dereferenced.
    const void* GetPrimitiveId() const { return PrimitiveId; }
    const void* GetOwnerId() const { return OwnerId; }

    const FMatrix& GetLocalToWorld() const { return LocalToWorld; }
    const FBox& GetBounds() const { return Bounds; }

protected:
    const void* PrimitiveId = nullptr;
    const void* OwnerId = nullptr;

    FMatrix LocalToWorld;
    FBox Bounds;
};

// Proxy for UStaticMeshComponent.
class FStaticMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
    FStaticMeshSceneProxy(const UStaticMeshComponent& Component);

    void UpdateFromComponent(const UPrimitiveComponent& Component) override;
    void Draw(FRHI& RHI, const FMatrix& View, const FMatrix& Projection, bool bRenderOutline) const override;

private:
    struct FSectionDrawInfo
    {
        uint32 FirstIndex = 0;
        uint32 IndexCount = 0;
        FRHIShaderResourceView* Texture = nullptr;
        FVector4 BaseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        FVector2D UVOffset = FVector2D(0.0f, 0.0f);
    };

    FRHIBuffer* VertexBuffer = nullptr;
    FRHIBuffer* IndexBuffer = nullptr;
    bool bHasVertexNormals = false;
    TArray<FSectionDrawInfo> Sections;
};
