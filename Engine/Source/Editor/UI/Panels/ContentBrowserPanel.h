#pragma once

#include "EditorPanel.h"
#include "EditorObjectContext.h"

class FAssetManager;
class UStaticMesh;
class UMaterial;
class UTexture2D;

class FContentBrowserPanel : public IEditorPanel
{
public:
    FContentBrowserPanel(FEditorObjectContext* InContext, FAssetManager* InAssetManager) : Context(InContext), AssetManager(InAssetManager) {}

    const char* GetPanelName() const override { return "Content Browser"; }

    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;

private:
    FEditorObjectContext* Context = nullptr;
    FAssetManager* AssetManager = nullptr;
};