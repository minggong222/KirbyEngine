#pragma once

#include "EditorPanel.h"

#include <Windows.h>
#include <filesystem>

struct FEditorObjectContext;
class FAssetManager;
class UCameraComponent;
class URenderer;

class FMainMenuPanel : public IEditorPanel
{
public:
    FMainMenuPanel(FEditorObjectContext* InContext, FAssetManager* InAssetManager, HWND InOwnerWindow, UCameraComponent* InViewportCamera, URenderer* InRenderer)
: Context(InContext), AssetManager(InAssetManager), OwnerWindow(InOwnerWindow), ViewportCamera(InViewportCamera), Renderer(InRenderer) {}

    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;

private:
    bool SelectSceneFile(bool bSaveDialog, std::filesystem::path& OutPath) const;
    void SaveScene(bool bSaveAs);
    void LoadScene();

    URenderer* Renderer = nullptr;
    
    FEditorObjectContext* Context = nullptr;
    FAssetManager* AssetManager = nullptr;
    HWND OwnerWindow = nullptr;
    UCameraComponent* ViewportCamera = nullptr;

    std::filesystem::path CurrentScenePath;
};