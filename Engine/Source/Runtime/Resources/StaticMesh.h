#pragma once
#include "Asset.h"
#include "FVertexSimple.h"
#include "StaticMeshData.h"

class URenderer;

class UStaticMesh : public UAsset
{
    DECLARE_CLASS( UStaticMesh, UAsset );
public:
    bool Initialize(URenderer& Renderer, TArray<FVertexSimple>&& SourceVertices);
    const FStaticMeshRenderData& GetRenderData() const { return RenderData; }
    ~UStaticMesh() override;
private:
    FStaticMeshRenderData RenderData;
};
