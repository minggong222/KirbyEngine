#pragma once
#include "EditorObjectContext.h"
#include "EditorPanel.h"

class FAssetManager;

class FDetailsPanel : public IEditorPanel
{
public:
    explicit FDetailsPanel(FEditorObjectContext* InContext, FAssetManager* InAssetManager) : Context(InContext), AssetManager(InAssetManager) {}
    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;
private:
    FEditorObjectContext* Context = nullptr;
    FAssetManager* AssetManager = nullptr;
};