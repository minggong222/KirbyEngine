#pragma once

#include "Asset.h"
#include "FVertexSimple.h"
#include "StaticMeshData.h"
#include "UObject/ObjectMacros.h"
#include "Resources/StaticMesh.generated.h"

class URenderer;

UCLASS()
class UStaticMesh : public UAsset
{
    GENERATED_BODY()

public:
    bool Initialize(URenderer& Renderer, FStaticMeshBuildData&& SourceData);
    bool Reinitialize(URenderer& Renderer, FStaticMeshBuildData&& SourceData);
    const FStaticMeshRenderData& GetRenderData() const { return RenderData; }
    ~UStaticMesh() override;

private:
    FStaticMeshRenderData RenderData;
};
