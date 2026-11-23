#pragma once

#include "EditorPanel.h"

#include <Windows.h>
#include <filesystem>

class FEditorObjectContext;
class FAssetManager;

class FMainMenuPanel : public IEditorPanel
{
public:
    FMainMenuPanel(FEditorObjectContext* InContext, FAssetManager* InAssetManager, HWND InOwnerWindow)
        : Context(InContext), AssetManager(InAssetManager), OwnerWindow(InOwnerWindow) {}

    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;

private:
    bool SelectSceneFile(bool bSaveDialog, std::filesystem::path& OutPath) const;
    void SaveScene(bool bSaveAs);
    void LoadScene();

    FEditorObjectContext* Context = nullptr;
    FAssetManager* AssetManager = nullptr;
    HWND OwnerWindow = nullptr;

    std::filesystem::path CurrentScenePath;
};