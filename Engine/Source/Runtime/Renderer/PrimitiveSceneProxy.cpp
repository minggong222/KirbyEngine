#include "Renderer/PrimitiveSceneProxy.h"

#include "RHI/RHI.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Resources/Material.h"
#include "Resources/StaticMesh.h"
#include "Resources/StaticMeshData.h"
#include "Resources/Texture2D.h"

FPrimitiveSceneProxy::FPrimitiveSceneProxy(const UPrimitiveComponent& Component)
    : PrimitiveId(&Component)
    , OwnerId(Component.GetOwner())
{
}

void FPrimitiveSceneProxy::UpdateFromComponent(const UPrimitiveComponent& Component)
{
    OwnerId = Component.GetOwner();
    LocalToWorld = Component.GetWorldMatrix();
    Bounds = Component.CalcBounds();
}

FStaticMeshSceneProxy::FStaticMeshSceneProxy(const UStaticMeshComponent& Component)
    : FPrimitiveSceneProxy(Component)
{
    UpdateFromComponent(Component);
}

void FStaticMeshSceneProxy::UpdateFromComponent(const UPrimitiveComponent& InComponent)
{
    FPrimitiveSceneProxy::UpdateFromComponent(InComponent);

    const UStaticMeshComponent& Component = static_cast<const UStaticMeshComponent&>(InComponent);

    VertexBuffer = nullptr;
    IndexBuffer = nullptr;
    bHasVertexNormals = false;
    Sections.Reset();

    const UStaticMesh* Mesh = Component.GetStaticMesh();

    if (Mesh == nullptr)
    {
        return;
    }

    const FStaticMeshRenderData& RenderData = Mesh->GetRenderData();

    if (RenderData.VertexBuffer == nullptr || RenderData.IndexBuffer == nullptr || RenderData.IndexCount == 0)
    {
        return;
    }

    VertexBuffer = RenderData.VertexBuffer;
    IndexBuffer = RenderData.IndexBuffer;
    bHasVertexNormals = RenderData.bHasVertexNormals;

    for (const FStaticMeshSection& Section : RenderData.Sections)
    {
        if (Section.IndexCount == 0 || Section.FirstIndex > RenderData.IndexCount || Section.IndexCount > RenderData.IndexCount - Section.FirstIndex)
        {
            continue;
        }

        FSectionDrawInfo Info;
        Info.FirstIndex = Section.FirstIndex;
        Info.IndexCount = Section.IndexCount;

        if (const UMaterial* Material = Component.GetMaterial(static_cast<int32>(Section.MaterialIndex)))
        {
            const UTexture2D* Texture = Material->GetBaseColorTexture();
            Info.Texture = Texture != nullptr && Texture->IsValid() ? Texture->GetResource().ShaderResourceView : nullptr;
            Info.BaseColor = Material->GetBaseColor();
            Info.UVOffset = Material->GetUVOffset();
        }

        Sections.Add(Info);
    }
}

void FStaticMeshSceneProxy::Draw(FRHI& RHI, const FMatrix& View, const FMatrix& Projection, bool bRenderOutline) const
{
    if (VertexBuffer == nullptr || IndexBuffer == nullptr)
    {
        return;
    }

    FMatrix WorldMatrix = LocalToWorld;

    if (bRenderOutline)
    {
        // Grow the hull around the bounds center (not the pivot) so the rim is even on every side.
        // The stencil mask hides the part that overlaps the object, leaving only the edge.
        constexpr float OutlineScale = 1.03f;

        if (Bounds.IsValid())
        {
            const FVector Center = (Bounds.GetMin() + Bounds.GetMax()) * 0.5f;
            WorldMatrix = WorldMatrix
                * FMatrix::CreateTranslation(-Center)
                * FMatrix::CreateScale(FVector(OutlineScale, OutlineScale, OutlineScale))
                * FMatrix::CreateTranslation(Center);
        }
        else
        {
            WorldMatrix = FMatrix::CreateScale(FVector(OutlineScale, OutlineScale, OutlineScale)) * WorldMatrix;
        }
    }

    for (const FSectionDrawInfo& Section : Sections)
    {
        if (bRenderOutline)
        {
            RHI.UpdateConstant(WorldMatrix, View, Projection, true, false, false, FVector4(1.0f, 1.0f, 1.0f, 1.0f), FVector2D(0.0f, 0.0f), bHasVertexNormals);
            RHI.RenderPrimitive(VertexBuffer, IndexBuffer, Section.IndexCount, true, nullptr, Section.FirstIndex);
        }
        else
        {
            RHI.UpdateConstant(WorldMatrix, View, Projection, false, Section.Texture != nullptr, false, Section.BaseColor, Section.UVOffset, bHasVertexNormals);
            RHI.RenderPrimitive(VertexBuffer, IndexBuffer, Section.IndexCount, false, Section.Texture, Section.FirstIndex);
        }
    }
}
