#pragma once

#include "Asset.h"
#include "Renderer/FVertexSimple.h"
#include "StaticMeshData.h"
#include "UObject/ObjectMacros.h"
#include "Resources/StaticMesh.generated.h"

class FRHI;

UCLASS()
class UStaticMesh : public UAsset
{
    GENERATED_BODY()

public:
    bool Initialize(FRHI& Renderer, FStaticMeshBuildData&& SourceData);
    bool Reinitialize(FRHI& Renderer, FStaticMeshBuildData&& SourceData);

    const FStaticMeshRenderData& GetRenderData() const
    {
        return RenderData;
    }

    void GetDependencies(TArray<UAsset*>& OutDependencies) const override;

    bool SupportsPackage() const override { return true; }
    void Serialize(FArchive& Ar) override;
    bool PostLoad(FAssetManager& AssetManager, FRHI& RHI) override;

    ~UStaticMesh() override;

private:
    FStaticMeshRenderData RenderData;

    // CPU copy of the vertices, kept so the mesh can be written to a package.
    TArray<FVertexSimple> SourceVertices;

    // Filled by Serialize when loading, consumed by PostLoad.
    FStaticMeshBuildData PendingLoadData;
};