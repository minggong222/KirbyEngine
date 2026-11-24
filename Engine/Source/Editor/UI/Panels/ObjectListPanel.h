#pragma once
#include "EditorObjectContext.h"
#include "EditorPanel.h"

class URenderer;
class FViewportPanel;
class FAssetManager;
class UStaticMesh;

class FObjectListPanel : public IEditorPanel
{
public:
    explicit FObjectListPanel(FEditorObjectContext* InContext, FAssetManager* InAssetManager, FViewportPanel* InViewportPanel, URenderer* InRenderer) : Context(InContext), AssetManager(InAssetManager), ViewportPanel(InViewportPanel), Renderer(InRenderer) {};
    bool Init() override;
    void Tick(float DeltaTime) override;
    const char* GetPanelName() const override { return "World Outliner"; }
    void OnRender() override;
    
    void DeleteSelectedObject();
    void DuplicateSelectedObject();
private:
    FEditorObjectContext* Context = nullptr;
    FAssetManager* AssetManager = nullptr;
    FViewportPanel* ViewportPanel = nullptr;
    URenderer* Renderer = nullptr;
    UStaticMesh* MeshToSpawn = nullptr;
};
