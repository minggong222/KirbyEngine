#pragma once

#include "EditorPanel.h"

#include <Windows.h>
#include <filesystem>

struct FEditorObjectContext;
class FAssetManager;
class UCameraComponent;

class FMainMenuPanel : public IEditorPanel
{
public:
    FMainMenuPanel(FEditorObjectContext* InContext, FAssetManager* InAssetManager, HWND InOwnerWindow, UCameraComponent* InViewportCamera)
    : Context(InContext), AssetManager(InAssetManager), OwnerWindow(InOwnerWindow), ViewportCamera(InViewportCamera) {}

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
    UCameraComponent* ViewportCamera = nullptr;

    std::filesystem::path CurrentScenePath;
};