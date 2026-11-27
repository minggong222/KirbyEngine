#pragma once

#include "EditorPanel.h"
#include "EditorObjectContext.h"

#include <string>
#include <unordered_map>
#include <vector>

class UAsset;
class FAssetManager;
class FRHI;
class UStaticMesh;
class UMaterial;
class UTexture2D;
struct FRHITexture;
struct FRHIShaderResourceView;

class FContentBrowserPanel : public IEditorPanel
{
public:
    FContentBrowserPanel(FEditorObjectContext* InContext, FAssetManager* InAssetManager, FRHI* InRenderer) : Context(InContext), AssetManager(InAssetManager), Renderer(InRenderer) {}
    ~FContentBrowserPanel() override;

    const char* GetPanelName() const override { return "Content Browser"; }

    bool Init() override;
    void Tick(float DeltaTime) override;
    void OnRender() override;

private:
    struct FAssetThumbnail
    {
        FRHITexture* Texture = nullptr;
        FRHIShaderResourceView* ShaderResourceView = nullptr;
    };

    void RefreshContent();
    void LoadRegisteredAssets();
    void ReleaseThumbnail(UAsset* Asset);
    void FlushDeferredThumbnailReleases();
    void ProcessPendingAssetOperations();
    void PruneThumbnailCache(const TArray<UAsset*>& LiveAssets);
    FRHIShaderResourceView* GetAssetThumbnail(UAsset* Asset, bool bForceGeneration = false);
    bool BuildAssetThumbnail(UAsset* Asset, FAssetThumbnail& OutThumbnail);
    void DrawAssetEditor();

    bool MoveAssetToFolder(UAsset* Asset, const std::string& DestinationFolder);
    bool MoveAssetToPath(UAsset* Asset, const std::string& DestinationAssetPath);
    bool MoveFolder(const std::string& OldFolder, const std::string& NewFolder);
    bool RenameAsset(UAsset* Asset, const std::string& NewName);
    bool RenameFolder(const std::string& Folder, const std::string& NewName);
    bool DeleteAsset(UAsset* Asset);
    bool DeleteFolder(const std::string& Folder);
    bool IsAssetReferencedByWorld(UAsset* Asset) const;

    FEditorObjectContext* Context = nullptr;
    FAssetManager* AssetManager = nullptr;
    FRHI* Renderer = nullptr;

    std::string CurrentFolder = "Content";
    UAsset* SelectedAsset = nullptr;
    UAsset* EditingAsset = nullptr;

    std::unordered_map<UAsset*, FAssetThumbnail> ThumbnailCache;
    std::vector<FAssetThumbnail> DeferredThumbnailReleases;
    UStaticMesh* PendingMeshReimport = nullptr;
    UTexture2D* PendingTextureReload = nullptr;
    UMaterial* PendingMaterialReload = nullptr;

    char AssetSearchBuffer[128] = {};
    char NewFolderBuffer[128] = {};
    char RenameBuffer[256] = {};
    std::string RenameTargetPath;
    std::string DeleteTargetPath;
    bool bRenameTargetIsFolder = false;
    bool bOpenDeleteModal = false;
    bool bAssetEditorOpen = false;
    bool bShowPreview = true;
    double LastRegistryRefreshTime = -1.0;
    int32 ThumbnailGenerationBudget = 0;
};
