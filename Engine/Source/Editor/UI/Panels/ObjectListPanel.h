#pragma once
#include "EditorObjectContext.h"
#include "EditorPanel.h"

class FViewportPanel;
class FAssetManager;
class UStaticMesh;

class FObjectListPanel : public IEditorPanel
{
public:
    explicit FObjectListPanel(FEditorObjectContext* InContext, FAssetManager* InAssetManager, FViewportPanel* InViewportPanel) : Context(InContext), AssetManager(InAssetManager), ViewportPanel(InViewportPanel) {};
    bool Init() override;
    void Tick(float DeltaTime) override;
    const char* GetPanelName() const override { return "Outliner"; }
    void OnRender() override;
    
    void DeleteSelectedObject();
    void DuplicateSelectedObject();
private:
    FEditorObjectContext* Context = nullptr;
    FAssetManager* AssetManager = nullptr;
    FViewportPanel* ViewportPanel = nullptr;
};
