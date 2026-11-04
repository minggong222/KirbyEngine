#pragma once
#include "EditorObjectContext.h"
#include "EditorPanel.h"

struct FStaticMeshData;
class URenderer;
class FObjectListPanel : public IEditorPanel
{
public:
    explicit FObjectListPanel(FEditorObjectContext* InContext, const FStaticMeshData* InCubeMeshData, const FStaticMeshData* InSphereMeshData) : Context(InContext), CubeMeshData(InCubeMeshData), SphereMeshData(InSphereMeshData) {}
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
private:
    FEditorObjectContext* Context = nullptr;
    const FStaticMeshData* CubeMeshData = nullptr;
    const FStaticMeshData* SphereMeshData = nullptr;
};
