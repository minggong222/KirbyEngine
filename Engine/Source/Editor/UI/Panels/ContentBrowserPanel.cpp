#include "ContentBrowserPanel.h"
#include "Core/PathUtils.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "Resources/Asset.h"
#include "Resources/AssetManager.h"
#include "Resources/StaticMesh.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"
#include "Resources/PrimitiveMeshAssets.h"
#include "Renderer/Renderer.h"
#include "Actors/Actor.h"
#include "Actors/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "World/World.h"
#include "World/Level.h"
#include "EditorUndoManager.h"
#include "OBJStaticMeshImporter.h"
#include "TextureAssetImporter.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <commdlg.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <system_error>
#include <unordered_set>
#include <vector>

#pragma comment(lib, "Comdlg32.lib")

namespace
{
    constexpr const char* AssetDragDropPayloadType = "KIRBY_ASSET";
    constexpr const char* FolderDragDropPayloadType = "KIRBY_FOLDER";
    constexpr uint32 ThumbnailWidth = 128;
    constexpr uint32 ThumbnailHeight = 128;

    std::string PathToUTF8(const std::filesystem::path& Path)
    {
        const std::u8string UTF8Path = Path.generic_u8string();
        return std::string(reinterpret_cast<const char*>(UTF8Path.data()), UTF8Path.size());
    }

    std::filesystem::path ToContentPath(const std::string& RelativePath)
    {
        return std::filesystem::current_path() / FPathUtils::FromUTF8(RelativePath);
    }

    std::string GetParentFolderPath(const std::string& Path)
    {
        const size_t LastSlash = Path.find_last_of('/');
        return LastSlash == std::string::npos ? std::string() : Path.substr(0, LastSlash);
    }

    std::string GetFolderName(const std::string& FolderPath)
    {
        const size_t LastSlash = FolderPath.find_last_of('/');
        return LastSlash == std::string::npos ? FolderPath : FolderPath.substr(LastSlash + 1);
    }

    std::string GetFileName(const std::string& AssetPath)
    {
        const size_t LastSlash = AssetPath.find_last_of('/');
        return LastSlash == std::string::npos ? AssetPath : AssetPath.substr(LastSlash + 1);
    }

    std::string GetAssetDisplayName(const std::string& AssetPath)
    {
        std::string FileName = GetFileName(AssetPath);
        const size_t ExtensionPosition = FileName.find_last_of('.');

        if (ExtensionPosition != std::string::npos)
        {
            FileName.resize(ExtensionPosition);
        }

        return FileName;
    }

    bool IsSameOrChildPath(const std::string& Path, const std::string& Parent)
    {
        return Path == Parent || Path.starts_with(Parent + "/");
    }

    bool HasChildFolder(const TArray<std::string>& Folders, const std::string& ParentFolder)
    {
        for (const std::string& Folder : Folders)
        {
            if (GetParentFolderPath(Folder) == ParentFolder)
            {
                return true;
            }
        }

        return false;
    }

    void CollectContentFolders(TArray<std::string>& OutFolders)
    {
        OutFolders.Reset();
        const std::filesystem::path ContentDirectory = std::filesystem::current_path() / "Content";
        std::error_code DirectoryError;

        if (!std::filesystem::is_directory(ContentDirectory, DirectoryError) || DirectoryError)
        {
            return;
        }

        OutFolders.Add("Content");

        for (std::filesystem::recursive_directory_iterator It(ContentDirectory, std::filesystem::directory_options::skip_permission_denied, DirectoryError), End; !DirectoryError && It != End; It.increment(DirectoryError))
        {
            std::error_code EntryError;

            if (It->is_directory(EntryError) && !EntryError)
            {
                OutFolders.AddUnique(PathToUTF8(It->path().lexically_relative(std::filesystem::current_path())));
            }
        }

        OutFolders.Sort();
    }

    enum class EContentBrowserIcon : uint8
    {
        Folder,
        StaticMesh,
        Material,
        Texture,
        Preview
    };

    void DrawContentBrowserIcon(ImDrawList* DrawList, EContentBrowserIcon Icon, const ImVec2& Center, float Size)
    {
        if (DrawList == nullptr)
        {
            return;
        }

        ImU32 Color = IM_COL32(210, 210, 210, 255);

        switch (Icon)
        {
        case EContentBrowserIcon::Folder:
            Color = IM_COL32(220, 180, 80, 255);
            DrawList->AddRectFilled(ImVec2(Center.x - Size * 0.42f, Center.y - Size * 0.25f), ImVec2(Center.x - Size * 0.05f, Center.y - Size * 0.08f), Color, 2.0f);
            DrawList->AddRectFilled(ImVec2(Center.x - Size * 0.45f, Center.y - Size * 0.12f), ImVec2(Center.x + Size * 0.45f, Center.y + Size * 0.30f), Color, 3.0f);
            break;

        case EContentBrowserIcon::StaticMesh:
        {
            Color = IM_COL32(110, 170, 220, 255);
            const ImVec2 Top(Center.x, Center.y - Size * 0.35f);
            const ImVec2 Left(Center.x - Size * 0.35f, Center.y - Size * 0.15f);
            const ImVec2 Right(Center.x + Size * 0.35f, Center.y - Size * 0.15f);
            const ImVec2 Middle(Center.x, Center.y + Size * 0.05f);
            const ImVec2 Bottom(Center.x, Center.y + Size * 0.35f);
            const ImVec2 BottomLeft(Center.x - Size * 0.35f, Center.y + Size * 0.15f);
            const ImVec2 BottomRight(Center.x + Size * 0.35f, Center.y + Size * 0.15f);
            DrawList->AddLine(Top, Left, Color, 2.0f);
            DrawList->AddLine(Top, Right, Color, 2.0f);
            DrawList->AddLine(Left, Middle, Color, 2.0f);
            DrawList->AddLine(Right, Middle, Color, 2.0f);
            DrawList->AddLine(Left, BottomLeft, Color, 2.0f);
            DrawList->AddLine(Right, BottomRight, Color, 2.0f);
            DrawList->AddLine(Middle, Bottom, Color, 2.0f);
            DrawList->AddLine(BottomLeft, Bottom, Color, 2.0f);
            DrawList->AddLine(BottomRight, Bottom, Color, 2.0f);
            break;
        }

        case EContentBrowserIcon::Material:
            Color = IM_COL32(180, 145, 220, 255);
            DrawList->AddCircleFilled(Center, Size * 0.30f, Color, 24);
            DrawList->AddCircle(Center, Size * 0.30f, IM_COL32(235, 235, 235, 255), 24, 1.5f);
            DrawList->AddCircleFilled(ImVec2(Center.x - Size * 0.10f, Center.y - Size * 0.12f), Size * 0.08f, IM_COL32(245, 245, 245, 220), 12);
            break;

        case EContentBrowserIcon::Texture:
            Color = IM_COL32(120, 190, 150, 255);
            DrawList->AddRectFilled(ImVec2(Center.x - Size * 0.38f, Center.y - Size * 0.30f), ImVec2(Center.x + Size * 0.38f, Center.y + Size * 0.30f), IM_COL32(55, 65, 65, 255), 2.0f);
            DrawList->AddRect(ImVec2(Center.x - Size * 0.38f, Center.y - Size * 0.30f), ImVec2(Center.x + Size * 0.38f, Center.y + Size * 0.30f), Color, 2.0f);
            DrawList->AddCircleFilled(ImVec2(Center.x + Size * 0.19f, Center.y - Size * 0.12f), Size * 0.07f, IM_COL32(235, 200, 100, 255), 12);
            DrawList->AddLine(ImVec2(Center.x - Size * 0.30f, Center.y + Size * 0.20f), ImVec2(Center.x - Size * 0.04f, Center.y - Size * 0.04f), Color, 2.0f);
            DrawList->AddLine(ImVec2(Center.x - Size * 0.04f, Center.y - Size * 0.04f), ImVec2(Center.x + Size * 0.10f, Center.y + Size * 0.12f), Color, 2.0f);
            DrawList->AddLine(ImVec2(Center.x + Size * 0.10f, Center.y + Size * 0.12f), ImVec2(Center.x + Size * 0.25f, Center.y - Size * 0.02f), Color, 2.0f);
            break;

        case EContentBrowserIcon::Preview:
        {
            const ImVec2 Left(Center.x - Size * 0.38f, Center.y);
            const ImVec2 Top(Center.x, Center.y - Size * 0.22f);
            const ImVec2 Right(Center.x + Size * 0.38f, Center.y);
            const ImVec2 Bottom(Center.x, Center.y + Size * 0.22f);
            DrawList->AddLine(Left, Top, Color, 2.0f);
            DrawList->AddLine(Top, Right, Color, 2.0f);
            DrawList->AddLine(Right, Bottom, Color, 2.0f);
            DrawList->AddLine(Bottom, Left, Color, 2.0f);
            DrawList->AddCircleFilled(Center, Size * 0.10f, Color, 16);
            break;
        }
        }
    }

    bool DrawContentBrowserTile(const char* Label, EContentBrowserIcon Icon, ID3D11ShaderResourceView* Thumbnail, bool bSelected, const ImVec2& TileSize)
    {
        const bool bClicked = ImGui::InvisibleButton("##AssetTile", TileSize);
        const bool bHovered = ImGui::IsItemHovered();
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        ImDrawList* DrawList = ImGui::GetWindowDrawList();
        const ImU32 Background = ImGui::GetColorU32(bSelected ? ImGuiCol_HeaderActive : bHovered ? ImGuiCol_HeaderHovered : ImGuiCol_FrameBg);

        DrawList->AddRectFilled(Min, Max, Background, 5.0f);

        if (bSelected)
        {
            DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImGuiCol_HeaderActive), 5.0f, 0, 1.5f);
        }

        const ImVec2 PreviewMin(Min.x + 8.0f, Min.y + 6.0f);
        const ImVec2 PreviewMax(Max.x - 8.0f, Max.y - 25.0f);

        if (Thumbnail != nullptr)
        {
            DrawList->AddImage((ImTextureID)Thumbnail, PreviewMin, PreviewMax);
        }
        else
        {
            DrawContentBrowserIcon(DrawList, Icon, ImVec2((Min.x + Max.x) * 0.5f, Min.y + 34.0f), 38.0f);
        }

        std::string DisplayLabel = Label != nullptr ? Label : "";

        while (!DisplayLabel.empty() && ImGui::CalcTextSize(DisplayLabel.c_str()).x > TileSize.x - 10.0f)
        {
            DisplayLabel.pop_back();
        }

        if (DisplayLabel != (Label != nullptr ? Label : ""))
        {
            DisplayLabel += "...";
        }

        const ImVec2 TextSize = ImGui::CalcTextSize(DisplayLabel.c_str());
        const ImVec2 TextPosition(Min.x + (TileSize.x - TextSize.x) * 0.5f, Max.y - 20.0f);
        DrawList->PushClipRect(Min, Max, true);
        DrawList->AddText(TextPosition, ImGui::GetColorU32(ImGuiCol_Text), DisplayLabel.c_str());
        DrawList->PopClipRect();
        return bClicked;
    }

    bool DrawPreviewToggleButton(bool bShowPreview)
    {
        ImGui::PushID("PreviewToggle");
        const bool bClicked = ImGui::InvisibleButton("##PreviewButton", ImVec2(30.0f, 26.0f));
        const ImVec2 Min = ImGui::GetItemRectMin();
        const ImVec2 Max = ImGui::GetItemRectMax();
        const bool bHovered = ImGui::IsItemHovered();
        const ImU32 Background = ImGui::GetColorU32(bShowPreview ? ImGuiCol_HeaderActive : bHovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
        ImGui::GetWindowDrawList()->AddRectFilled(Min, Max, Background, 4.0f);
        DrawContentBrowserIcon(ImGui::GetWindowDrawList(), EContentBrowserIcon::Preview, ImVec2((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f), 22.0f);

        if (bHovered)
        {
            ImGui::SetTooltip("Toggle Preview");
        }

        ImGui::PopID();
        return bClicked;
    }

    const char* GetAssetTypeLabel(UAsset* Asset)
    {
        if (Cast<UStaticMesh>(Asset) != nullptr) return "Static Mesh";
        if (Cast<UMaterial>(Asset) != nullptr) return "Material";
        if (Cast<UTexture2D>(Asset) != nullptr) return "Texture";
        return "Asset";
    }

    EContentBrowserIcon GetAssetIcon(UAsset* Asset)
    {
        if (Cast<UStaticMesh>(Asset) != nullptr) return EContentBrowserIcon::StaticMesh;
        if (Cast<UMaterial>(Asset) != nullptr) return EContentBrowserIcon::Material;
        if (Cast<UTexture2D>(Asset) != nullptr) return EContentBrowserIcon::Texture;
        return EContentBrowserIcon::Folder;
    }

    UStaticMesh* SelectAndImportOBJ(FAssetManager& AssetManager, URenderer& Renderer)
    {
        wchar_t FileName[32768] = {};
        static constexpr wchar_t Filter[] = L"Wavefront OBJ (*.obj)\0*.obj\0All Files (*.*)\0*.*\0";
        OPENFILENAMEW Dialog = {};
        Dialog.lStructSize = sizeof(Dialog);
        Dialog.lpstrFilter = Filter;
        Dialog.lpstrFile = FileName;
        Dialog.nMaxFile = static_cast<DWORD>(std::size(FileName));
        Dialog.lpstrTitle = L"Import Static Mesh";
        Dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&Dialog))
        {
            return nullptr;
        }

        return OBJImporter::ImportExternalOBJ(std::filesystem::path(FileName), AssetManager, Renderer);
    }

    UMaterial* SelectAndImportTexture(FAssetManager& AssetManager, URenderer& Renderer)
    {
        wchar_t FileName[32768] = {};
        static constexpr wchar_t Filter[] = L"Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.gif)\0*.png;*.jpg;*.jpeg;*.bmp;*.tif;*.tiff;*.gif\0All Files (*.*)\0*.*\0";
        OPENFILENAMEW Dialog = {};
        Dialog.lStructSize = sizeof(Dialog);
        Dialog.lpstrFilter = Filter;
        Dialog.lpstrFile = FileName;
        Dialog.nMaxFile = static_cast<DWORD>(std::size(FileName));
        Dialog.lpstrTitle = L"Import Texture";
        Dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

        if (!GetOpenFileNameW(&Dialog))
        {
            return nullptr;
        }

        return TextureImporter::ImportExternalTexture(std::filesystem::path(FileName), AssetManager, Renderer);
    }

    void CollectTextureAssetsUnderPath(FAssetManager& AssetManager, const std::string& Prefix, std::vector<std::pair<UTexture2D*, std::string>>& OutTextures)
    {
        TArray<UTexture2D*> Textures;
        AssetManager.GetAssets(Textures);

        for (UTexture2D* Texture : Textures)
        {
            if (Texture != nullptr && IsSameOrChildPath(Texture->GetAssetPath().ToStd(), Prefix))
            {
                OutTextures.emplace_back(Texture, Texture->GetAssetPath().ToStd());
            }
        }
    }

}

FContentBrowserPanel::~FContentBrowserPanel()
{
    for (auto& Pair : ThumbnailCache)
    {
        if (Pair.second.ShaderResourceView != nullptr)
        {
            Pair.second.ShaderResourceView->Release();
        }

        if (Pair.second.Texture != nullptr)
        {
            Pair.second.Texture->Release();
        }
    }

    ThumbnailCache.clear();

    for (FAssetThumbnail& Thumbnail : DeferredThumbnailReleases)
    {
        if (Thumbnail.ShaderResourceView != nullptr) Thumbnail.ShaderResourceView->Release();
        if (Thumbnail.Texture != nullptr) Thumbnail.Texture->Release();
    }

    DeferredThumbnailReleases.clear();
}

bool FContentBrowserPanel::Init()
{
    if (Context == nullptr || AssetManager == nullptr || Renderer == nullptr)
    {
        return false;
    }

    RefreshContent();
    return true;
}

void FContentBrowserPanel::Tick(float DeltaTime)
{
}

void FContentBrowserPanel::RefreshContent()
{
    if (AssetManager == nullptr || Renderer == nullptr)
    {
        return;
    }

    AssetManager->RefreshAssetRegistry();
    LoadRegisteredAssets();
    AssetManager->RefreshAssetRegistry();
}

void FContentBrowserPanel::LoadRegisteredAssets()
{
    if (AssetManager == nullptr || Renderer == nullptr)
    {
        return;
    }

    TArray<FAssetRegistryEntry> Entries;
    AssetManager->GetAssetRegistryEntries(Entries);

    for (const FAssetRegistryEntry& Entry : Entries)
    {
        const FString AssetPath(Entry.AssetPath);

        if (Entry.AssetType == "Static Mesh")
        {
            OBJImporter::LoadOBJAsset(AssetPath, *AssetManager, *Renderer);
        }
        else if (Entry.AssetType == "Texture")
        {
            TextureImporter::LoadTextureAsset(AssetPath, *AssetManager, *Renderer);
        }
        else if (Entry.AssetType == "Material")
        {
            TextureImporter::LoadMaterialAsset(AssetPath, *AssetManager, *Renderer);
        }
    }

    TArray<UTexture2D*> Textures;
    AssetManager->GetAssets(Textures);

    for (UTexture2D* Texture : Textures)
    {
        if (Texture != nullptr)
        {
            TextureImporter::LoadTextureMaterial(Texture->GetAssetPath(), *AssetManager, *Renderer);
        }
    }
}

void FContentBrowserPanel::ReleaseThumbnail(UAsset* Asset)
{
    const auto It = ThumbnailCache.find(Asset);

    if (It == ThumbnailCache.end())
    {
        return;
    }

    DeferredThumbnailReleases.push_back(It->second);
    ThumbnailCache.erase(It);
}

void FContentBrowserPanel::FlushDeferredThumbnailReleases()
{
    for (FAssetThumbnail& Thumbnail : DeferredThumbnailReleases)
    {
        if (Thumbnail.ShaderResourceView != nullptr) Thumbnail.ShaderResourceView->Release();
        if (Thumbnail.Texture != nullptr) Thumbnail.Texture->Release();
    }

    DeferredThumbnailReleases.clear();
}

void FContentBrowserPanel::ProcessPendingAssetOperations()
{
    if (PendingTextureReload != nullptr)
    {
        UTexture2D* Texture = PendingTextureReload;
        PendingTextureReload = nullptr;

        if (AssetManager->FindAny(Texture->GetAssetPath()) == Texture && TextureImporter::ReloadTextureAsset(Texture, *Renderer))
        {
            TArray<UAsset*> ReferencingMaterials;
            AssetManager->GetReferencers(Texture, ReferencingMaterials);
            for (UAsset* Referencer : ReferencingMaterials) ReleaseThumbnail(Referencer);
        }
    }

    if (PendingMeshReimport != nullptr)
    {
        UStaticMesh* Mesh = PendingMeshReimport;
        PendingMeshReimport = nullptr;

        if (AssetManager != nullptr && Renderer != nullptr && AssetManager->FindAny(Mesh->GetAssetPath()) == Mesh && OBJImporter::ReimportOBJAsset(Mesh, *AssetManager, *Renderer))
        {
            ReleaseThumbnail(Mesh);
        }
    }

    if (PendingMaterialReload != nullptr)
    {
        UMaterial* Material = PendingMaterialReload;
        PendingMaterialReload = nullptr;

        if (AssetManager->FindAny(Material->GetAssetPath()) == Material && TextureImporter::ReloadMaterialAsset(Material, *AssetManager, *Renderer))
        {
            ReleaseThumbnail(Material);
        }
    }
}

void FContentBrowserPanel::PruneThumbnailCache(const TArray<UAsset*>& LiveAssets)
{
    std::unordered_set<UAsset*> LiveSet;

    for (UAsset* Asset : LiveAssets)
    {
        if (Asset != nullptr)
        {
            LiveSet.insert(Asset);
        }
    }

    for (auto It = ThumbnailCache.begin(); It != ThumbnailCache.end();)
    {
        if (LiveSet.contains(It->first))
        {
            ++It;
            continue;
        }

        DeferredThumbnailReleases.push_back(It->second);
        It = ThumbnailCache.erase(It);
    }
}

bool FContentBrowserPanel::BuildAssetThumbnail(UAsset* Asset, FAssetThumbnail& OutThumbnail)
{
    if (Asset == nullptr || Renderer == nullptr)
    {
        return false;
    }

    UStaticMesh* Mesh = Cast<UStaticMesh>(Asset);
    UMaterial* Material = Cast<UMaterial>(Asset);

    if (Material != nullptr)
    {
        TArray<UStaticMesh*> Meshes;
        AssetManager->GetAssets(Meshes);

        for (UStaticMesh* Candidate : Meshes)
        {
            if (Candidate != nullptr && Candidate->GetAssetPath().ToStd() == BuiltinAssetPaths::Sphere)
            {
                Mesh = Candidate;
                break;
            }
        }
    }

    if (Mesh == nullptr)
    {
        return false;
    }

    const FStaticMeshRenderData& RenderData = Mesh->GetRenderData();

    if (!RenderData.LocalBounds.IsValid() || RenderData.VertexBuffer == nullptr || RenderData.IndexBuffer == nullptr || RenderData.IndexCount == 0)
    {
        return false;
    }

    Renderer->CreateAssetPreviewRenderTarget(ThumbnailWidth, ThumbnailHeight);

    if (Renderer->GetAssetPreviewSRV() == nullptr || Renderer->GetAssetPreviewTexture() == nullptr)
    {
        return false;
    }

    const FVector& Min = RenderData.LocalBounds.GetMin();
    const FVector& Max = RenderData.LocalBounds.GetMax();
    const FVector Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f, (Min.z + Max.z) * 0.5f);
    const float ExtentX = (Max.x - Min.x) * 0.5f;
    const float ExtentY = (Max.y - Min.y) * 0.5f;
    const float ExtentZ = (Max.z - Min.z) * 0.5f;
    const float MaxExtent = (std::max)(ExtentX, (std::max)(ExtentY, ExtentZ));
    const float PreviewScale = MaxExtent > 0.0001f ? 1.3f / MaxExtent : 1.0f;
    const FMatrix World = FMatrix::CreateTranslation(FVector(-Center.x, -Center.y, -Center.z)) * FMatrix::CreateScale(FVector(PreviewScale, PreviewScale, PreviewScale));
    const FMatrix View = FMatrix::CreateLookAt(FVector(-4.0f, 0.0f, 0.0f), FVector::Zero, FVector(0.0f, 0.0f, 1.0f));
    const FMatrix Projection = FMatrix::CreatePerspectiveFieldOfView(45.0f * 0.01745329252f, 1.0f, 0.1f, 100.0f);
    UTexture2D* BaseColorTexture = Material != nullptr ? Material->GetBaseColorTexture() : nullptr;
    ID3D11ShaderResourceView* TextureSRV = BaseColorTexture != nullptr && BaseColorTexture->IsValid() ? BaseColorTexture->GetResource().ShaderResourceView : nullptr;

    
    Renderer->BeginAssetPreviewRender();
    Renderer->PrepareShader();

    for (const FStaticMeshSection& Section : RenderData.Sections)
    {
        if (Section.IndexCount == 0 || Section.FirstIndex > RenderData.IndexCount || Section.IndexCount > RenderData.IndexCount - Section.FirstIndex)
        {
            continue;
        }

        UMaterial* SectionMaterial = Material;

        if (SectionMaterial == nullptr && Section.MaterialIndex < RenderData.Materials.Num())
        {
            SectionMaterial = RenderData.Materials[Section.MaterialIndex];
        }

        UTexture2D* SectionTexture = SectionMaterial != nullptr ? SectionMaterial->GetBaseColorTexture() : nullptr;
        ID3D11ShaderResourceView* SectionTextureSRV = SectionTexture != nullptr && SectionTexture->IsValid() ? SectionTexture->GetResource().ShaderResourceView : nullptr;

        const FVector4 SectionMaterialColor = SectionMaterial != nullptr ? SectionMaterial->GetBaseColor() : FVector4(1.0f, 1.0f, 1.0f, 1.0f);
        const FVector2D SectionUVOffset = SectionMaterial != nullptr ? SectionMaterial->GetUVOffset() : FVector2D(0.0f, 0.0f);

        Renderer->UpdateConstant(World, View, Projection, false, SectionTextureSRV != nullptr, false, SectionMaterialColor, SectionUVOffset, RenderData.bHasVertexNormals);
        Renderer->RenderPrimitive(RenderData.VertexBuffer, RenderData.IndexBuffer, Section.IndexCount, false, SectionTextureSRV, Section.FirstIndex);
    }

    Renderer->EndAssetPreviewRender();


    ID3D11Device* Device = Renderer->GetDevice();
    ID3D11DeviceContext* DeviceContext = Renderer->GetDeviceContext();
    ID3D11Texture2D* SourceTexture = Renderer->GetAssetPreviewTexture();

    if (Device == nullptr || DeviceContext == nullptr || SourceTexture == nullptr)
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC TextureDesc = {};
    SourceTexture->GetDesc(&TextureDesc);
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.MiscFlags = 0;
    TextureDesc.CPUAccessFlags = 0;
    TextureDesc.Usage = D3D11_USAGE_DEFAULT;

    if (FAILED(Device->CreateTexture2D(&TextureDesc, nullptr, &OutThumbnail.Texture)))
    {
        OutThumbnail.Texture = nullptr;
        return false;
    }

    DeviceContext->CopyResource(OutThumbnail.Texture, SourceTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC ViewDesc = {};
    ViewDesc.Format = TextureDesc.Format;
    ViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    ViewDesc.Texture2D.MipLevels = TextureDesc.MipLevels;

    if (FAILED(Device->CreateShaderResourceView(OutThumbnail.Texture, &ViewDesc, &OutThumbnail.ShaderResourceView)))
    {
        OutThumbnail.Texture->Release();
        OutThumbnail.Texture = nullptr;
        OutThumbnail.ShaderResourceView = nullptr;
        return false;
    }

    return true;
}

ID3D11ShaderResourceView* FContentBrowserPanel::GetAssetThumbnail(UAsset* Asset, bool bForceGeneration)
{
    if (Asset == nullptr)
    {
        return nullptr;
    }

    if (UTexture2D* Texture = Cast<UTexture2D>(Asset))
    {
        return Texture->IsValid() ? Texture->GetResource().ShaderResourceView : nullptr;
    }

    const auto Existing = ThumbnailCache.find(Asset);

    if (Existing != ThumbnailCache.end())
    {
        return Existing->second.ShaderResourceView;
    }

    if (!bForceGeneration && ThumbnailGenerationBudget <= 0)
    {
        return nullptr;
    }

    if (!bForceGeneration)
    {
        --ThumbnailGenerationBudget;
    }
    FAssetThumbnail Thumbnail;

    if (!BuildAssetThumbnail(Asset, Thumbnail))
    {
        return nullptr;
    }

    return ThumbnailCache.emplace(Asset, Thumbnail).first->second.ShaderResourceView;
}

void FContentBrowserPanel::DrawAssetEditor()
{
    if (!bAssetEditorOpen || EditingAsset == nullptr)
    {
        return;
    }

    const char* WindowTitle = Cast<UStaticMesh>(EditingAsset) != nullptr ? "Static Mesh Editor" : Cast<UMaterial>(EditingAsset) != nullptr ? "Material Editor" : "Texture Viewer";
    bool bOpen = bAssetEditorOpen;

    if (ImGui::Begin(WindowTitle, &bOpen))
    {
        ImGui::TextWrapped("%s", EditingAsset->GetAssetPath().c_str());
        ImGui::Separator();

        if (UTexture2D* Texture = Cast<UTexture2D>(EditingAsset))
        {
            if (Texture->IsValid())
            {
                const float LongestSide = static_cast<float>((std::max)(Texture->GetSizeX(), Texture->GetSizeY()));
                const ImVec2 AvailablePreviewSize = ImGui::GetContentRegionAvail();
                const float MaxPreviewWidth = (std::max)(1.0f, AvailablePreviewSize.x);
                const float MaxPreviewHeight = (std::max)(1.0f, AvailablePreviewSize.y - 60.0f);
                const float Scale = LongestSide > 0.0f ? (std::min)(1.0f, (std::min)(MaxPreviewWidth / Texture->GetSizeX(), MaxPreviewHeight / Texture->GetSizeY())) : 1.0f;
                ImGui::Image((ImTextureID)Texture->GetResource().ShaderResourceView, ImVec2(Texture->GetSizeX() * Scale, Texture->GetSizeY() * Scale));
                ImGui::Text("Resolution: %u x %u", Texture->GetSizeX(), Texture->GetSizeY());
                if (ImGui::Button("Reload Texture From File"))
                {
                    PendingTextureReload = Texture;
                }
            }
            else
            {
                ImGui::TextDisabled("Texture resource is not available.");
            }
        }
        else if (UStaticMesh* Mesh = Cast<UStaticMesh>(EditingAsset))
        {
            const FStaticMeshRenderData& Data = Mesh->GetRenderData();
            ImGui::Text("Vertices: %u", Data.VertexCount);
            ImGui::Text("Indices: %u", Data.IndexCount);
            ImGui::TextDisabled("The source OBJ is stored under Content. Reimport after changing that file.");

            if (ImGui::Button("Reimport Source OBJ"))
            {
                PendingMeshReimport = Mesh;
            }

            if (ID3D11ShaderResourceView* MeshPreview = GetAssetThumbnail(Mesh, true))
            {
                const float PreviewSide = (std::max)(1.0f, (std::min)(320.0f, ImGui::GetContentRegionAvail().x));
                ImGui::Image((ImTextureID)MeshPreview, ImVec2(PreviewSide, PreviewSide));
            }
        }
        else if (UMaterial* Material = Cast<UMaterial>(EditingAsset))
        {
            TArray<UTexture2D*> Textures;
            AssetManager->GetAssets(Textures);
            UTexture2D* CurrentTexture = Material->GetBaseColorTexture();
            const char* CurrentLabel = CurrentTexture != nullptr ? CurrentTexture->GetAssetPath().c_str() : "None";

            if (ImGui::BeginCombo("Base Color Texture", CurrentLabel))
            {
                if (ImGui::Selectable("None", CurrentTexture == nullptr))
                {
                    Material->SetBaseColorTexture(nullptr);
                    ReleaseThumbnail(Material);
                }

                for (UTexture2D* Texture : Textures)
                {
                    if (Texture == nullptr) continue;
                    const bool bSelected = Texture == CurrentTexture;
                    if (ImGui::Selectable(Texture->GetAssetPath().c_str(), bSelected))
                    {
                        Material->SetBaseColorTexture(Texture);
                        ReleaseThumbnail(Material);
                    }
                }

                ImGui::EndCombo();
            }

            if (ID3D11ShaderResourceView* MaterialPreview = GetAssetThumbnail(EditingAsset, true))
            {
                ImGui::Image((ImTextureID)MaterialPreview, ImVec2(320.0f, 320.0f));
            }

            if (ImGui::Button("Save Material"))
            {
                if (TextureImporter::SaveMaterialAsset(Material)) AssetManager->RefreshAssetRegistry();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload Material"))
            {
                PendingMaterialReload = Material;
            }
        }
    }

    ImGui::End();
    bAssetEditorOpen = bOpen;

    if (!bAssetEditorOpen)
    {
        EditingAsset = nullptr;
    }
}

bool FContentBrowserPanel::MoveAssetToPath(UAsset* Asset, const std::string& DestinationAssetPath)
{
    if (Asset == nullptr || Cast<UMaterial>(Asset) != nullptr || DestinationAssetPath.empty())
    {
        return false;
    }

    const std::string OldAssetPath = Asset->GetAssetPath().ToStd();
    const std::filesystem::path DestinationRelativePath = FPathUtils::FromUTF8(DestinationAssetPath).lexically_normal();

    if (OldAssetPath == DestinationAssetPath)
    {
        return true;
    }

    if (DestinationRelativePath.is_absolute() || !PathToUTF8(DestinationRelativePath).starts_with("Content/"))
    {
        return false;
    }

    for (const auto& Part : DestinationRelativePath)
    {
        if (Part == "..")
        {
            return false;
        }
    }

    if (UAsset* ExistingAsset = AssetManager->FindAny(FString(DestinationAssetPath)))
    {
        if (ExistingAsset != Asset)
        {
            return false;
        }
    }

    const std::filesystem::path OldFile = ToContentPath(OldAssetPath);
    const std::filesystem::path NewFile = ToContentPath(DestinationAssetPath);

    if (!std::filesystem::is_regular_file(OldFile) || std::filesystem::exists(NewFile) || NewFile.parent_path().empty())
    {
        return false;
    }

    std::error_code FileSystemError;
    std::filesystem::create_directories(NewFile.parent_path(), FileSystemError);

    if (FileSystemError)
    {
        return false;
    }

    std::filesystem::path OldSidecar;
    std::filesystem::path NewSidecar;
    FString OldMaterialPath;
    FString NewMaterialPath;
    UMaterial* SidecarMaterial = nullptr;
    std::vector<UMaterial*> MaterialsToSave;
    bool bSidecarMoved = false;
    bool bMaterialRepathed = false;

    if (Cast<UTexture2D>(Asset) != nullptr)
    {
        TArray<UAsset*> TextureReferencers;
        AssetManager->GetReferencers(Asset, TextureReferencers);

        for (UAsset* Referencer : TextureReferencers)
        {
            UMaterial* ReferencingMaterial = Cast<UMaterial>(Referencer);

            if (ReferencingMaterial == nullptr)
            {
                return false;
            }

            if (std::find(MaterialsToSave.begin(), MaterialsToSave.end(), ReferencingMaterial) == MaterialsToSave.end())
            {
                MaterialsToSave.push_back(ReferencingMaterial);
            }
        }
        OldMaterialPath = TextureImporter::GetMaterialAssetPath(FString(OldAssetPath));
        NewMaterialPath = TextureImporter::GetMaterialAssetPath(FString(DestinationAssetPath));
        OldSidecar = ToContentPath(OldMaterialPath.ToStd());
        NewSidecar = ToContentPath(NewMaterialPath.ToStd());
        SidecarMaterial = AssetManager->Find<UMaterial>(OldMaterialPath);

        if (std::filesystem::exists(NewSidecar))
        {
            return false;
        }

        if (std::filesystem::exists(OldSidecar) && SidecarMaterial == nullptr)
        {
            return false;
        }

        if (UMaterial* ExistingMaterial = AssetManager->Find<UMaterial>(NewMaterialPath))
        {
            if (ExistingMaterial != SidecarMaterial)
            {
                return false;
            }
        }

        if (SidecarMaterial != nullptr && std::find(MaterialsToSave.begin(), MaterialsToSave.end(), SidecarMaterial) == MaterialsToSave.end())
        {
            MaterialsToSave.push_back(SidecarMaterial);
        }
    }

    auto RollbackMove = [&]()
    {
        AssetManager->RepathAsset(Asset, OldAssetPath);

        if (bMaterialRepathed && SidecarMaterial != nullptr)
        {
            AssetManager->RepathAsset(SidecarMaterial, OldMaterialPath.ToStd());
        }

        if (bSidecarMoved)
        {
            FileSystemError.clear();
            std::filesystem::rename(NewSidecar, OldSidecar, FileSystemError);
        }

        FileSystemError.clear();
        std::filesystem::rename(NewFile, OldFile, FileSystemError);

        for (UMaterial* Material : MaterialsToSave)
        {
            TextureImporter::SaveMaterialAsset(Material);
        }

        AssetManager->RefreshAssetRegistry();
    };

    std::filesystem::rename(OldFile, NewFile, FileSystemError);

    if (FileSystemError)
    {
        return false;
    }

    if (!AssetManager->RepathAsset(Asset, DestinationAssetPath))
    {
        FileSystemError.clear();
        std::filesystem::rename(NewFile, OldFile, FileSystemError);
        return false;
    }

    if (!OldSidecar.empty() && std::filesystem::exists(OldSidecar))
    {
        std::filesystem::create_directories(NewSidecar.parent_path(), FileSystemError);

        if (!FileSystemError)
        {
            std::filesystem::rename(OldSidecar, NewSidecar, FileSystemError);
            bSidecarMoved = !FileSystemError;
        }

        if (!bSidecarMoved)
        {
            RollbackMove();
            return false;
        }
    }

    if (SidecarMaterial != nullptr)
    {
        if (!AssetManager->RepathAsset(SidecarMaterial, NewMaterialPath.ToStd()))
        {
            RollbackMove();
            return false;
        }

        bMaterialRepathed = true;
    }

    for (UMaterial* Material : MaterialsToSave)
    {
        if (!TextureImporter::SaveMaterialAsset(Material))
        {
            RollbackMove();
            return false;
        }
    }

    AssetManager->RefreshAssetRegistry();
    return true;
}

bool FContentBrowserPanel::MoveAssetToFolder(UAsset* Asset, const std::string& DestinationFolder)
{
    if (Asset == nullptr || Cast<UMaterial>(Asset) != nullptr)
    {
        return false;
    }

    const std::string Folder = DestinationFolder.empty() ? "Content" : DestinationFolder;
    const std::string OldPath = Asset->GetAssetPath().ToStd();
    const std::string Destination = Folder + "/" + GetFileName(OldPath);

    if (GetParentFolderPath(OldPath) == Folder)
    {
        return true;
    }

    return MoveAssetToPath(Asset, Destination);
}

bool FContentBrowserPanel::MoveFolder(const std::string& OldFolder, const std::string& NewFolder)
{
    if (OldFolder.empty() || NewFolder.empty() || OldFolder == "Content" || !OldFolder.starts_with("Content/") || !NewFolder.starts_with("Content/") || IsSameOrChildPath(NewFolder, OldFolder))
    {
        return false;
    }

    TArray<FAssetRegistryEntry> RegistryEntries;
    AssetManager->GetAssetRegistryEntries(RegistryEntries);

    for (const FAssetRegistryEntry& Entry : RegistryEntries)
    {
        if (Entry.AssetType == "Material" && IsSameOrChildPath(Entry.AssetPath, OldFolder))
        {
            return false;
        }
    }

    const std::filesystem::path OldDirectory = ToContentPath(OldFolder);
    const std::filesystem::path NewDirectory = ToContentPath(NewFolder);

    if (!std::filesystem::is_directory(OldDirectory) || std::filesystem::exists(NewDirectory))
    {
        return false;
    }

    std::vector<std::pair<UTexture2D*, std::string>> TexturesToMove;
    CollectTextureAssetsUnderPath(*AssetManager, OldFolder, TexturesToMove);

    struct FMaterialMove
    {
        UMaterial* Material = nullptr;
        FString OldAssetPath;
        FString NewAssetPath;
        std::filesystem::path OldFile;
        std::filesystem::path NewFile;
        bool bFileMoved = false;
        bool bAssetRepathed = false;
    };

    std::vector<FMaterialMove> MaterialMoves;
    MaterialMoves.reserve(TexturesToMove.size());
    std::vector<UMaterial*> MaterialsToSave;

    for (const auto& TextureEntry : TexturesToMove)
    {
        TArray<UAsset*> TextureReferencers;
        AssetManager->GetReferencers(TextureEntry.first, TextureReferencers);

        for (UAsset* Referencer : TextureReferencers)
        {
            UMaterial* ReferencingMaterial = Cast<UMaterial>(Referencer);

            if (ReferencingMaterial == nullptr)
            {
                return false;
            }

            if (std::find(MaterialsToSave.begin(), MaterialsToSave.end(), ReferencingMaterial) == MaterialsToSave.end())
            {
                MaterialsToSave.push_back(ReferencingMaterial);
            }
        }
        const FString OldMaterialPath = TextureImporter::GetMaterialAssetPath(FString(TextureEntry.second));
        const std::string NewTexturePath = NewFolder + TextureEntry.second.substr(OldFolder.size());
        const FString NewMaterialPath = TextureImporter::GetMaterialAssetPath(FString(NewTexturePath));
        const std::filesystem::path OldMaterialFile = ToContentPath(OldMaterialPath.ToStd());
        const std::filesystem::path NewMaterialFile = ToContentPath(NewMaterialPath.ToStd());
        UMaterial* Material = AssetManager->Find<UMaterial>(OldMaterialPath);

        if (std::filesystem::exists(NewMaterialFile))
        {
            return false;
        }

        if (std::filesystem::exists(OldMaterialFile) && Material == nullptr)
        {
            return false;
        }

        if (UMaterial* ExistingMaterial = AssetManager->Find<UMaterial>(NewMaterialPath))
        {
            if (ExistingMaterial != Material)
            {
                return false;
            }
        }

        FMaterialMove Move;
        Move.Material = Material;
        Move.OldAssetPath = OldMaterialPath;
        Move.NewAssetPath = NewMaterialPath;
        Move.OldFile = OldMaterialFile;
        Move.NewFile = NewMaterialFile;

        if (Material != nullptr && std::find(MaterialsToSave.begin(), MaterialsToSave.end(), Material) == MaterialsToSave.end())
        {
            MaterialsToSave.push_back(Material);
        }

        MaterialMoves.push_back(std::move(Move));
    }

    std::error_code FileSystemError;
    std::filesystem::create_directories(NewDirectory.parent_path(), FileSystemError);

    if (FileSystemError)
    {
        return false;
    }

    const std::string PreviousCurrentFolder = CurrentFolder;
    std::filesystem::rename(OldDirectory, NewDirectory, FileSystemError);

    if (FileSystemError)
    {
        return false;
    }

    if (!AssetManager->RepathAssets(OldFolder, NewFolder))
    {
        FileSystemError.clear();
        std::filesystem::rename(NewDirectory, OldDirectory, FileSystemError);
        return false;
    }

    auto RollbackFolderMove = [&]()
    {
        for (auto It = MaterialMoves.rbegin(); It != MaterialMoves.rend(); ++It)
        {
            if (It->bFileMoved)
            {
                FileSystemError.clear();
                std::filesystem::rename(It->NewFile, It->OldFile, FileSystemError);
            }
        }

        AssetManager->RepathAssets(NewFolder, OldFolder);

        for (FMaterialMove& Move : MaterialMoves)
        {
            if (Move.bAssetRepathed && Move.Material != nullptr)
            {
                AssetManager->RepathAsset(Move.Material, Move.OldAssetPath.ToStd());
            }
        }

        for (UMaterial* Material : MaterialsToSave)
        {
            TextureImporter::SaveMaterialAsset(Material);
        }

        FileSystemError.clear();
        std::filesystem::rename(NewDirectory, OldDirectory, FileSystemError);
        CurrentFolder = PreviousCurrentFolder;
        AssetManager->RefreshAssetRegistry();
    };

    for (FMaterialMove& Move : MaterialMoves)
    {
        if (std::filesystem::exists(Move.OldFile))
        {
            std::filesystem::create_directories(Move.NewFile.parent_path(), FileSystemError);

            if (FileSystemError)
            {
                RollbackFolderMove();
                return false;
            }

            std::filesystem::rename(Move.OldFile, Move.NewFile, FileSystemError);

            if (FileSystemError)
            {
                RollbackFolderMove();
                return false;
            }

            Move.bFileMoved = true;
        }

        if (Move.Material != nullptr)
        {
            if (!AssetManager->RepathAsset(Move.Material, Move.NewAssetPath.ToStd()))
            {
                RollbackFolderMove();
                return false;
            }

            Move.bAssetRepathed = true;
        }
    }

    for (UMaterial* Material : MaterialsToSave)
    {
        if (!TextureImporter::SaveMaterialAsset(Material))
        {
            RollbackFolderMove();
            return false;
        }
    }

    if (IsSameOrChildPath(CurrentFolder, OldFolder))
    {
        CurrentFolder = NewFolder + CurrentFolder.substr(OldFolder.size());
    }

    AssetManager->RefreshAssetRegistry();
    return true;
}

bool FContentBrowserPanel::RenameAsset(UAsset* Asset, const std::string& NewName)
{
    if (Asset == nullptr || Cast<UMaterial>(Asset) != nullptr || NewName.empty() || NewName.find_first_of("/\\") != std::string::npos || NewName == "." || NewName == "..")
    {
        return false;
    }

    const std::string OldPath = Asset->GetAssetPath().ToStd();
    const std::string FileName = GetFileName(OldPath);
    const size_t ExtensionPosition = FileName.find_last_of('.');
    const std::string Extension = ExtensionPosition == std::string::npos ? std::string() : FileName.substr(ExtensionPosition);
    return MoveAssetToPath(Asset, GetParentFolderPath(OldPath) + "/" + NewName + Extension);
}

bool FContentBrowserPanel::RenameFolder(const std::string& Folder, const std::string& NewName)
{
    if (NewName.empty() || NewName.find_first_of("/\\") != std::string::npos || NewName == "." || NewName == "..")
    {
        return false;
    }

    const std::string NewFolder = GetParentFolderPath(Folder) + "/" + NewName;
    return MoveFolder(Folder, NewFolder);
}

bool FContentBrowserPanel::IsAssetReferencedByWorld(UAsset* Asset) const
{
    if (Asset == nullptr || Context == nullptr || Context->World == nullptr)
    {
        return false;
    }

    ULevel* Level = Context->World->GetPersistentLevel();

    if (Level == nullptr)
    {
        return false;
    }

    for (AActor* Actor : Level->GetActors())
    {
        if (Actor == nullptr)
        {
            continue;
        }

        for (UActorComponent* ActorComponent : Actor->GetComponents())
        {
            UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ActorComponent);

            if (MeshComponent == nullptr)
            {
                continue;
            }

            if (Cast<UStaticMesh>(Asset) != nullptr && MeshComponent->GetStaticMesh() == Asset)
            {
                return true;
            }

            UMaterial* TargetMaterial = Cast<UMaterial>(Asset);

            if (TargetMaterial != nullptr && MeshComponent->GetMaterials().Contains(TargetMaterial))
            {
                return true;
            }
        }
    }

    return false;
}

bool FContentBrowserPanel::DeleteAsset(UAsset* Asset)
{
    if (Asset == nullptr || Cast<UMaterial>(Asset) != nullptr || IsAssetReferencedByWorld(Asset))
    {
        return false;
    }

    TArray<UAsset*> Referencers;
    AssetManager->GetReferencers(Asset, Referencers);
    UMaterial* AssociatedMaterial = nullptr;
    std::filesystem::path MaterialFile;

    if (UTexture2D* Texture = Cast<UTexture2D>(Asset))
    {
        AssociatedMaterial = AssetManager->Find<UMaterial>(TextureImporter::GetMaterialAssetPath(Texture->GetAssetPath()));
        MaterialFile = ToContentPath(TextureImporter::GetMaterialAssetPath(Texture->GetAssetPath()).ToStd());

        for (UAsset* Referencer : Referencers)
        {
            if (Referencer != AssociatedMaterial || IsAssetReferencedByWorld(Referencer))
            {
                return false;
            }
        }

        if (AssociatedMaterial != nullptr)
        {
            TArray<UAsset*> MaterialReferencers;
            AssetManager->GetReferencers(AssociatedMaterial, MaterialReferencers);

            if (!MaterialReferencers.IsEmpty() || IsAssetReferencedByWorld(AssociatedMaterial))
            {
                return false;
            }
        }
    }
    else if (!Referencers.IsEmpty())
    {
        return false;
    }

    const std::filesystem::path AssetFile = ToContentPath(Asset->GetAssetPath().ToStd());
    std::error_code FileSystemError;

    if (!std::filesystem::is_regular_file(AssetFile, FileSystemError) || FileSystemError)
    {
        return false;
    }

    if (Context != nullptr && Context->UndoManager != nullptr)
    {
        Context->UndoManager->Clear();
    }

    if (AssociatedMaterial != nullptr)
    {
        std::filesystem::remove(MaterialFile, FileSystemError);

        if (FileSystemError)
        {
            return false;
        }

        if (SelectedAsset == AssociatedMaterial) SelectedAsset = nullptr;
        if (EditingAsset == AssociatedMaterial) { EditingAsset = nullptr; bAssetEditorOpen = false; }
        ReleaseThumbnail(AssociatedMaterial);

        if (!AssetManager->RemoveAsset(AssociatedMaterial))
        {
            return false;
        }
    }
    else if (Cast<UTexture2D>(Asset) != nullptr)
    {
        std::filesystem::remove(MaterialFile, FileSystemError);

        if (FileSystemError)
        {
            return false;
        }
    }

    FileSystemError.clear();

    if (!std::filesystem::remove(AssetFile, FileSystemError) || FileSystemError)
    {
        return false;
    }

    ReleaseThumbnail(Asset);

    if (!AssetManager->RemoveAsset(Asset))
    {
        return false;
    }

    if (SelectedAsset == Asset) SelectedAsset = nullptr;
    if (EditingAsset == Asset) { EditingAsset = nullptr; bAssetEditorOpen = false; }
    AssetManager->RefreshAssetRegistry();
    return true;
}

bool FContentBrowserPanel::DeleteFolder(const std::string& Folder)
{
    if (Folder.empty() || Folder == "Content")
    {
        return false;
    }

    const std::filesystem::path Directory = ToContentPath(Folder);
    std::error_code FileSystemError;

    if (!std::filesystem::is_directory(Directory, FileSystemError) || FileSystemError || !std::filesystem::is_empty(Directory, FileSystemError) || FileSystemError)
    {
        return false;
    }

    const bool bRemoved = std::filesystem::remove(Directory, FileSystemError) && !FileSystemError;

    if (bRemoved)
    {
        if (IsSameOrChildPath(CurrentFolder, Folder))
        {
            CurrentFolder = GetParentFolderPath(Folder);
            if (CurrentFolder.empty()) CurrentFolder = "Content";
        }

        AssetManager->RefreshAssetRegistry();
    }

    return bRemoved;
}

void FContentBrowserPanel::OnRender()
{
    if (!IsOpen() || Context == nullptr || AssetManager == nullptr || Renderer == nullptr)
    {
        return;
    }

    bool bIsOpen = IsOpen();

    if (!ImGui::Begin("Content Browser", &bIsOpen))
    {
        SetOpen(bIsOpen);
        ImGui::End();
        return;
    }

    FlushDeferredThumbnailReleases();
    ProcessPendingAssetOperations();

    const double CurrentTime = ImGui::GetTime();
    if (LastRegistryRefreshTime < 0.0 || CurrentTime - LastRegistryRefreshTime >= 1.0)
    {
        LastRegistryRefreshTime = CurrentTime;
        RefreshContent();
    }

    bool bRequestNewFolderPopup = false;
    bool bRequestRenamePopup = false;
    bool bRequestDeletePopup = false;
    bool bRequestOpenEditor = false;
    bool bRequestRefresh = false;
    UAsset* PendingDeleteAsset = nullptr;
    std::string PendingDeleteFolder;

    if (ImGui::Button("Import")) ImGui::OpenPopup("ImportAssetMenu");

    if (ImGui::BeginPopup("ImportAssetMenu"))
    {
        if (ImGui::MenuItem("Static Mesh (.obj)"))
        {
            if (UStaticMesh* ImportedAsset = SelectAndImportOBJ(*AssetManager, *Renderer))
            {
                SelectedAsset = ImportedAsset;
                CurrentFolder = GetParentFolderPath(ImportedAsset->GetAssetPath().ToStd());
                RefreshContent();
            }
        }

        if (ImGui::MenuItem("Texture"))
        {
            if (UMaterial* ImportedMaterial = SelectAndImportTexture(*AssetManager, *Renderer))
            {
                UTexture2D* ImportedTexture = ImportedMaterial->GetBaseColorTexture();
                SelectedAsset = ImportedTexture != nullptr ? static_cast<UAsset*>(ImportedTexture) : static_cast<UAsset*>(ImportedMaterial);
                CurrentFolder = GetParentFolderPath(SelectedAsset->GetAssetPath().ToStd());
                RefreshContent();
            }
        }

        ImGui::EndPopup();
    }

    if (ImGui::GetContentRegionAvail().x > 300.0f) ImGui::SameLine();

    if (ImGui::Button("New Folder"))
    {
        NewFolderBuffer[0] = '\0';
        bRequestNewFolderPopup = true;
    }

    if (ImGui::GetContentRegionAvail().x > 180.0f) ImGui::SameLine();

    if (ImGui::Button("Refresh")) bRequestRefresh = true;

    if (ImGui::GetContentRegionAvail().x > 260.0f) ImGui::SameLine();

    const float SearchRowWidth = ImGui::GetContentRegionAvail().x;

    if (SearchRowWidth >= 140.0f)
    {
        ImGui::SetNextItemWidth(SearchRowWidth - 38.0f);
        ImGui::InputTextWithHint("##AssetSearch", "Search Assets...", AssetSearchBuffer, IM_ARRAYSIZE(AssetSearchBuffer));
        ImGui::SameLine();
        if (DrawPreviewToggleButton(bShowPreview)) bShowPreview = !bShowPreview;
    }
    else
    {
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputTextWithHint("##AssetSearch", "Search Assets...", AssetSearchBuffer, IM_ARRAYSIZE(AssetSearchBuffer));
        if (DrawPreviewToggleButton(bShowPreview)) bShowPreview = !bShowPreview;
    }

    if (bRequestRefresh) RefreshContent();

    ImGui::Separator();

    TArray<UStaticMesh*> StaticMeshes;
    TArray<UMaterial*> Materials;
    TArray<UTexture2D*> Textures;
    AssetManager->GetAssets(StaticMeshes);
    AssetManager->GetAssets(Materials);
    AssetManager->GetAssets(Textures);

    TArray<UAsset*> AllAssets;
    FAssetRegistryEntry RegisteredEntry;

    for (UStaticMesh* Asset : StaticMeshes)
    {
        if (Asset != nullptr && AssetManager->FindAssetRegistryEntry(Asset->GetAssetPath().ToStd(), RegisteredEntry)) AllAssets.Add(Asset);
    }
    for (UMaterial* Asset : Materials)
    {
        if (Asset != nullptr && AssetManager->FindAssetRegistryEntry(Asset->GetAssetPath().ToStd(), RegisteredEntry)) AllAssets.Add(Asset);
    }
    for (UTexture2D* Asset : Textures)
    {
        if (Asset != nullptr && AssetManager->FindAssetRegistryEntry(Asset->GetAssetPath().ToStd(), RegisteredEntry)) AllAssets.Add(Asset);
    }
    AllAssets.Sort([](const UAsset* A, const UAsset* B) { return A->GetAssetPath() < B->GetAssetPath(); });
    PruneThumbnailCache(AllAssets);
    ThumbnailGenerationBudget = 3;

    if (SelectedAsset != nullptr)
    {
        GetAssetThumbnail(SelectedAsset);
    }

    if (EditingAsset != nullptr && EditingAsset != SelectedAsset)
    {
        GetAssetThumbnail(EditingAsset);
    }

    TArray<std::string> FolderPaths;
    CollectContentFolders(FolderPaths);
    const std::string SearchText = AssetSearchBuffer;

    const float BrowserContentWidth = ImGui::GetContentRegionAvail().x;
    const bool bDrawPreview = bShowPreview && BrowserContentWidth >= 600.0f;
    const float SourcesColumnWidth = std::clamp(BrowserContentWidth * 0.22f, 100.0f, 180.0f);
    const float PreviewColumnWidth = std::clamp(BrowserContentWidth * 0.28f, 150.0f, 270.0f);

    const int32 LayoutColumnCount = bDrawPreview ? 3 : 2;
    const char* LayoutID = bDrawPreview ? "ContentBrowserLayoutWithPreview" : "ContentBrowserLayoutWithoutPreview";

    if (ImGui::BeginTable(LayoutID, LayoutColumnCount, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Sources", ImGuiTableColumnFlags_WidthFixed, SourcesColumnWidth);
        ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthStretch);
        if (bDrawPreview) ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, PreviewColumnWidth);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::BeginChild("##ContentSources", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        ImGui::PushID("ContentRoot");
        if (ImGui::Selectable("Content", CurrentFolder == "Content", ImGuiSelectableFlags_SpanAvailWidth)) CurrentFolder = "Content";
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadType))
            {
                UAsset* DroppedAsset = *static_cast<UAsset**>(Payload->Data);
                MoveAssetToFolder(DroppedAsset, "Content");
            }
            if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(FolderDragDropPayloadType))
            {
                const std::string DroppedFolder(static_cast<const char*>(Payload->Data));
                MoveFolder(DroppedFolder, "Content/" + GetFolderName(DroppedFolder));
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();

        auto DrawFolderTree = [&](auto&& Self, const std::string& ParentFolder) -> void
        {
            for (const std::string& Folder : FolderPaths)
            {
                if (GetParentFolderPath(Folder) != ParentFolder)
                {
                    continue;
                }

                ImGui::PushID(Folder.c_str());
                const std::string FolderName = GetFolderName(Folder);
                const bool bSelected = CurrentFolder == Folder;
                const bool bHasChildren = HasChildFolder(FolderPaths, Folder);
                ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                if (bSelected) Flags |= ImGuiTreeNodeFlags_Selected;
                if (!bHasChildren) Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                const ImVec2 IconPosition = ImGui::GetCursorScreenPos();
                ImGui::Dummy(ImVec2(18.0f, 18.0f));
                DrawContentBrowserIcon(ImGui::GetWindowDrawList(), EContentBrowserIcon::Folder, ImVec2(IconPosition.x + 9.0f, IconPosition.y + 9.0f), 15.0f);
                ImGui::SameLine(0.0f, 2.0f);
                const std::string TreeLabel = FolderName + "##" + Folder;
                const bool bExpanded = ImGui::TreeNodeEx(TreeLabel.c_str(), Flags);

                if (ImGui::IsItemClicked())
                {
                    CurrentFolder = Folder;
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadType))
                    {
                        UAsset* DroppedAsset = *static_cast<UAsset**>(Payload->Data);
                        MoveAssetToFolder(DroppedAsset, Folder);
                    }
                    if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(FolderDragDropPayloadType))
                    {
                        const std::string DroppedFolder(static_cast<const char*>(Payload->Data));
                        if (DroppedFolder != Folder) MoveFolder(DroppedFolder, Folder + "/" + GetFolderName(DroppedFolder));
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload(FolderDragDropPayloadType, Folder.c_str(), Folder.size() + 1);
                    ImGui::Text("Move folder: %s", Folder.c_str());
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginPopupContextItem("FolderTreeContextMenu"))
                {
                    if (ImGui::MenuItem("Open")) CurrentFolder = Folder;
                    if (ImGui::MenuItem("Rename"))
                    {
                        RenameTargetPath = Folder;
                        bRenameTargetIsFolder = true;
                        strncpy_s(RenameBuffer, GetFolderName(Folder).c_str(), _TRUNCATE);
                        bRequestRenamePopup = true;
                    }
                    if (ImGui::MenuItem("Delete Empty Folder"))
                    {
                        PendingDeleteFolder = Folder;
                        bRequestDeletePopup = true;
                    }
                    ImGui::EndPopup();
                }

                if (bExpanded && bHasChildren)
                {
                    Self(Self, Folder);
                    ImGui::TreePop();
                }

                ImGui::PopID();
            }
        };

        DrawFolderTree(DrawFolderTree, "Content");
        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);
        ImGui::BeginChild("##ContentAssets", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

        if (CurrentFolder != "Content")
        {
            if (ImGui::SmallButton(".."))
            {
                CurrentFolder = GetParentFolderPath(CurrentFolder);
                if (CurrentFolder.empty()) CurrentFolder = "Content";
                SelectedAsset = nullptr;
            }
            ImGui::SameLine();
        }

        ImGui::TextUnformatted(CurrentFolder.c_str());
        ImGui::Separator();

        const float AvailableWidth = ImGui::GetContentRegionAvail().x;
        const int32 ColumnCount = (std::max)(1, static_cast<int32>(AvailableWidth / 132.0f));
        if (ImGui::BeginTable("AssetTiles", ColumnCount, ImGuiTableFlags_SizingStretchSame))
        {
            int32 VisibleItemCount = 0;

            for (const std::string& Folder : FolderPaths)
            {
                if (Folder == CurrentFolder || GetParentFolderPath(Folder) != CurrentFolder) continue;
                const std::string FolderName = GetFolderName(Folder);
                if (!SearchText.empty() && Folder.find(SearchText) == std::string::npos) continue;

                ImGui::TableNextColumn();
                ImGui::PushID(Folder.c_str());
                const ImVec2 TileSize(ImGui::GetContentRegionAvail().x, 96.0f);
                if (DrawContentBrowserTile(FolderName.c_str(), EContentBrowserIcon::Folder, nullptr, false, TileSize))
                {
                    CurrentFolder = Folder;
                    SelectedAsset = nullptr;
                }

                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", Folder.c_str());
                if (ImGui::BeginDragDropSource())
                {
                    ImGui::SetDragDropPayload(FolderDragDropPayloadType, Folder.c_str(), Folder.size() + 1);
                    ImGui::Text("Move folder: %s", Folder.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadType))
                    {
                        UAsset* DroppedAsset = *static_cast<UAsset**>(Payload->Data);
                        MoveAssetToFolder(DroppedAsset, Folder);
                    }
                    if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(FolderDragDropPayloadType))
                    {
                        const std::string DroppedFolder(static_cast<const char*>(Payload->Data));
                        if (DroppedFolder != Folder) MoveFolder(DroppedFolder, Folder + "/" + GetFolderName(DroppedFolder));
                    }
                    ImGui::EndDragDropTarget();
                }
                if (ImGui::BeginPopupContextItem("FolderContextMenu"))
                {
                    if (ImGui::MenuItem("Open")) CurrentFolder = Folder;
                    if (ImGui::MenuItem("Rename"))
                    {
                        RenameTargetPath = Folder;
                        bRenameTargetIsFolder = true;
                        strncpy_s(RenameBuffer, GetFolderName(Folder).c_str(), _TRUNCATE);
                        bRequestRenamePopup = true;
                    }
                    if (ImGui::MenuItem("Delete Empty Folder"))
                    {
                        PendingDeleteFolder = Folder;
                        bRequestDeletePopup = true;
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
                ++VisibleItemCount;
            }

            for (UAsset* Asset : AllAssets)
            {
                if (Asset == nullptr) continue;
                const std::string AssetPath = Asset->GetAssetPath().ToStd();
                if (GetParentFolderPath(AssetPath) != CurrentFolder) continue;
                if (!SearchText.empty() && AssetPath.find(SearchText) == std::string::npos) continue;

                const std::string DisplayName = GetAssetDisplayName(AssetPath);
                ImGui::TableNextColumn();
                ImGui::PushID(Asset);
                const bool bSelected = SelectedAsset == Asset;
                const ImVec2 TileSize(ImGui::GetContentRegionAvail().x, 96.0f);
                ID3D11ShaderResourceView* ThumbnailSRV = GetAssetThumbnail(Asset);

                if (DrawContentBrowserTile(DisplayName.c_str(), GetAssetIcon(Asset), ThumbnailSRV, bSelected, TileSize))
                {
                    SelectedAsset = Asset;
                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s\n%s", GetAssetTypeLabel(Asset), AssetPath.c_str());
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        EditingAsset = Asset;
                        bRequestOpenEditor = true;
                    }
                }

                if (ImGui::BeginDragDropSource())
                {
                    UAsset* DraggedAsset = Asset;
                    ImGui::SetDragDropPayload(AssetDragDropPayloadType, &DraggedAsset, sizeof(UAsset*));
                    ImGui::Text("%s", DisplayName.c_str());
                    ImGui::EndDragDropSource();
                }

                if (UMaterial* TargetMaterial = Cast<UMaterial>(Asset))
                {
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadType))
                        {
                            UAsset* DroppedAsset = *static_cast<UAsset**>(Payload->Data);
                            if (UTexture2D* DroppedTexture = Cast<UTexture2D>(DroppedAsset))
                            {
                                TargetMaterial->SetBaseColorTexture(DroppedTexture);
                                ReleaseThumbnail(TargetMaterial);
                                TextureImporter::SaveMaterialAsset(TargetMaterial);
                                AssetManager->RefreshAssetRegistry();
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                if (ImGui::BeginPopupContextItem("AssetContextMenu"))
                {
                    SelectedAsset = Asset;
                    if (ImGui::MenuItem("Open"))
                    {
                        EditingAsset = Asset;
                        bRequestOpenEditor = true;
                    }
                    if (ImGui::MenuItem("Copy Asset Path")) ImGui::SetClipboardText(AssetPath.c_str());
                    ImGui::BeginDisabled(Cast<UMaterial>(Asset) != nullptr);
                    if (ImGui::MenuItem("Rename"))
                    {
                        RenameTargetPath = AssetPath;
                        bRenameTargetIsFolder = false;
                        strncpy_s(RenameBuffer, DisplayName.c_str(), _TRUNCATE);
                        bRequestRenamePopup = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::BeginDisabled(Cast<UMaterial>(Asset) != nullptr);
                    if (ImGui::MenuItem("Delete"))
                    {
                        PendingDeleteAsset = Asset;
                        bRequestDeletePopup = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::BeginDisabled(Cast<UMaterial>(Asset) != nullptr);
                    if (ImGui::BeginMenu("Move To"))
                    {
                        for (const std::string& Folder : FolderPaths)
                        {
                            if (ImGui::MenuItem(Folder.c_str(), nullptr, false, Folder != GetParentFolderPath(AssetPath)))
                            {
                                MoveAssetToFolder(Asset, Folder);
                            }
                        }
                        ImGui::EndMenu();
                    }
                    ImGui::EndDisabled();
                    ImGui::EndPopup();
                }

                ImGui::PopID();
                ++VisibleItemCount;
            }

            ImGui::EndTable();

            if (VisibleItemCount == 0) ImGui::TextDisabled("This folder is empty.");
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(AssetDragDropPayloadType))
            {
                UAsset* DroppedAsset = *static_cast<UAsset**>(Payload->Data);
                MoveAssetToFolder(DroppedAsset, CurrentFolder);
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::EndChild();

        if (bDrawPreview)
        {
            ImGui::TableSetColumnIndex(2);
            ImGui::BeginChild("##AssetPreview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

            if (SelectedAsset != nullptr)
            {
                const std::string DisplayName = GetAssetDisplayName(SelectedAsset->GetAssetPath().ToStd());
                ImGui::TextWrapped("%s", DisplayName.c_str());
                ImGui::Text("Type: %s", GetAssetTypeLabel(SelectedAsset));
                ImGui::TextWrapped("Path: %s", SelectedAsset->GetAssetPath().c_str());
                ImGui::Separator();

                if (UTexture2D* Texture = Cast<UTexture2D>(SelectedAsset))
                {
                    if (Texture->IsValid())
                    {
                        const float LongestSide = static_cast<float>((std::max)(Texture->GetSizeX(), Texture->GetSizeY()));
                        const float Scale = LongestSide > 0.0f ? (std::min)(1.0f, 220.0f / LongestSide) : 1.0f;
                        ImGui::Image((ImTextureID)Texture->GetResource().ShaderResourceView, ImVec2(Texture->GetSizeX() * Scale, Texture->GetSizeY() * Scale));
                        ImGui::Text("Resolution: %u x %u", Texture->GetSizeX(), Texture->GetSizeY());
                    }
                }
                else if (UStaticMesh* Mesh = Cast<UStaticMesh>(SelectedAsset))
                {
                    const FStaticMeshRenderData& Data = Mesh->GetRenderData();
                    ImGui::Text("Vertices: %u", Data.VertexCount);
                    ImGui::Text("Indices: %u", Data.IndexCount);
                    if (ID3D11ShaderResourceView* SRV = GetAssetThumbnail(SelectedAsset, true))
                    {
                        const float PreviewSide = (std::max)(1.0f, (std::min)(220.0f, ImGui::GetContentRegionAvail().x));
                        ImGui::Image((ImTextureID)SRV, ImVec2(PreviewSide, PreviewSide));
                    }
                }
                else if (UMaterial* Material = Cast<UMaterial>(SelectedAsset))
                {
                    UTexture2D* Texture = Material->GetBaseColorTexture();
                    ImGui::TextWrapped("Base Color: %s", Texture != nullptr ? Texture->GetAssetPath().c_str() : "None");
                    TArray<UAsset*> Dependencies;
                    TArray<UAsset*> Referencers;
                    AssetManager->GetDependencies(Material, Dependencies);
                    AssetManager->GetReferencers(Material, Referencers);
                    ImGui::Text("Dependencies: %d", Dependencies.Num());
                    ImGui::Text("Referencers: %d", Referencers.Num());
                    if (ID3D11ShaderResourceView* Thumbnail = GetAssetThumbnail(SelectedAsset, true)) ImGui::Image((ImTextureID)Thumbnail, ImVec2(220.0f, 220.0f));
                    if (ImGui::Button("Open Material Editor"))
                    {
                        EditingAsset = Material;
                        bRequestOpenEditor = true;
                    }
                }

                if (ImGui::Button("Open Editor"))
                {
                    EditingAsset = SelectedAsset;
                    bRequestOpenEditor = true;
                }
            }
            else
            {
                ImGui::TextDisabled("Select an asset to preview.");
            }

            ImGui::EndChild();
        }

        ImGui::EndTable();
    }

    ImGui::End();
    SetOpen(bIsOpen);

    if (bRequestNewFolderPopup) ImGui::OpenPopup("Create Content Folder");
    if (bRequestRenamePopup) ImGui::OpenPopup("Rename Content Item");
    if (PendingDeleteAsset != nullptr)
    {
        DeleteTargetPath = PendingDeleteAsset->GetAssetPath().ToStd();
        bOpenDeleteModal = true;
    }
    else if (!PendingDeleteFolder.empty())
    {
        DeleteTargetPath = PendingDeleteFolder;
        bOpenDeleteModal = true;
    }
    if (bRequestDeletePopup && bOpenDeleteModal) ImGui::OpenPopup("Delete Content Item");
    if (bRequestOpenEditor) bAssetEditorOpen = true;

    if (ImGui::BeginPopupModal("Create Content Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Folder Name", NewFolderBuffer, IM_ARRAYSIZE(NewFolderBuffer));
        if (ImGui::Button("Create"))
        {
            const std::string NewName = NewFolderBuffer;
            if (!NewName.empty() && NewName.find_first_of("/\\") == std::string::npos && NewName != "." && NewName != "..")
            {
                const std::string ParentFolder = CurrentFolder.empty() ? "Content" : CurrentFolder;
                const std::filesystem::path NewFolderPath = ToContentPath(ParentFolder + "/" + NewName);
                std::error_code FileSystemError;
                std::filesystem::create_directories(NewFolderPath, FileSystemError);
                if (!FileSystemError) RefreshContent();
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Rename Content Item", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("New Name", RenameBuffer, IM_ARRAYSIZE(RenameBuffer));
        if (ImGui::Button("Rename"))
        {
            const bool bRenamed = bRenameTargetIsFolder ? RenameFolder(RenameTargetPath, RenameBuffer) : RenameAsset(AssetManager->FindAny(FString(RenameTargetPath)), RenameBuffer);
            if (bRenamed) RefreshContent();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (ImGui::BeginPopupModal("Delete Content Item", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("Delete '%s'? This cannot be undone.", DeleteTargetPath.c_str());
        if (ImGui::Button("Delete"))
        {
            if (UAsset* Asset = AssetManager->FindAny(FString(DeleteTargetPath))) DeleteAsset(Asset);
            else DeleteFolder(DeleteTargetPath);
            RefreshContent();
            bOpenDeleteModal = false;
            DeleteTargetPath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            bOpenDeleteModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    DrawAssetEditor();
}
