#pragma once

#include "EditorPanel.h"
#include "EditorObjectContext.h"

class FAssetManager;
class URenderer;
class UStaticMesh;
class UMaterial;
class UTexture2D;

class FContentBrowserPanel : public IEditorPanel
{
public:
    FContentBrowserPanel(FEditorObjectContext* InContext, FAssetManager* InAssetManager, URenderer* InRenderer) : Context(InContext), AssetManager(InAssetManager), Renderer(InRenderer) {}

    const char* GetPanelName() const override { return "Content Browser"; }

    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;

private:
    FEditorObjectContext* Context = nullptr;
    FAssetManager* AssetManager = nullptr;
    URenderer* Renderer = nullptr;
};