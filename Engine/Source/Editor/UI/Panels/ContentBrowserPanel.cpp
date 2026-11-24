#include "ContentBrowserPanel.h"

#include "imgui.h"

#include "Resources/AssetManager.h"
#include "Resources/StaticMesh.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"
#include "EditorUndoManager.h"
#include "imgui_internal.h"
#include "Actors/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "World/World.h"
#include "OBJStaticMeshImporter.h"
#include "TextureAssetImporter.h"
#include "Renderer/Renderer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Resources/Asset.h"
#include "Resources/PrimitiveMeshAssets.h"
#include <algorithm>
#include <cstring>

#include <Windows.h>
#include <commdlg.h>
#include <filesystem>
#include <iterator>

#pragma comment(lib, "Comdlg32.lib")


namespace
{
    
    std::string GetParentFolderPath(const std::string& Path)
    {
        const size_t LastSlash = Path.find_last_of('/');

        if (LastSlash == std::string::npos || LastSlash == 0)
        {
            return {};
        }

        return Path.substr(0, LastSlash);
    }

    std::string GetFolderName(const std::string& FolderPath)
    {
        const size_t LastSlash = FolderPath.find_last_of('/');
        return LastSlash == std::string::npos ? FolderPath : FolderPath.substr(LastSlash + 1);
    }

    std::string GetAssetDisplayName(const std::string& AssetPath)
    {
        const size_t LastSlash = AssetPath.find_last_of('/');
        std::string FileName = LastSlash == std::string::npos ? AssetPath : AssetPath.substr(LastSlash + 1);

        const size_t ExtensionPosition = FileName.find_last_of('.');

        if (ExtensionPosition != std::string::npos)
        {
            FileName.resize(ExtensionPosition);
        }

        return FileName;
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

    void DrawContentFolderTree(const std::string& ParentFolder, const TArray<std::string>& Folders, std::string& CurrentFolder)
    {
        for (const std::string& Folder : Folders)
        {
            if (GetParentFolderPath(Folder) != ParentFolder)
            {
                continue;
            }

            const bool bHasChildren = HasChildFolder(Folders, Folder);
            const bool bSelected = CurrentFolder == Folder;

            ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

            if (bSelected)
            {
                Flags |= ImGuiTreeNodeFlags_Selected;
            }

            if (!bHasChildren)
            {
                Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            }

            if (ParentFolder.empty())
            {
                Flags |= ImGuiTreeNodeFlags_DefaultOpen;
            }

            ImGui::PushID(Folder.c_str());

            const std::string FolderName = GetFolderName(Folder);
            const bool bExpanded = ImGui::TreeNodeEx("##Folder", Flags, "%s", FolderName.c_str());

            if (ImGui::IsItemClicked())
            {
                CurrentFolder = Folder;
            }

            if (bExpanded && bHasChildren)
            {
                DrawContentFolderTree(Folder, Folders, CurrentFolder);
                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    const char* GetAssetTypeLabel(UAsset* Asset)
    {
        if (Cast<UStaticMesh>(Asset) != nullptr) return "Static Mesh";
        if (Cast<UMaterial>(Asset) != nullptr) return "Material";
        if (Cast<UTexture2D>(Asset) != nullptr) return "Texture";
        return "Asset";
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
        Dialog.lpstrTitle = L"Import OBJ";
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
    
    void DrawStaticMeshPreview(URenderer& Renderer, UStaticMesh* Mesh, UMaterial* Material)
    {
        if (Mesh == nullptr)
        {
            ImGui::TextDisabled("Preview mesh is unavailable.");
            return;
        }

        const FStaticMeshRenderData& RenderData = Mesh->GetRenderData();

        if (!RenderData.LocalBounds.IsValid() || RenderData.VertexBuffer == nullptr || RenderData.IndexBuffer == nullptr || RenderData.IndexCount == 0)
        {
            ImGui::TextDisabled("Mesh render data is unavailable.");
            return;
        }

        Renderer.CreateAssetPreviewRenderTarget(256, 256);

        ID3D11ShaderResourceView* PreviewSRV = Renderer.GetAssetPreviewSRV();

        if (PreviewSRV == nullptr)
        {
            ImGui::TextDisabled("Preview render target is unavailable.");
            return;
        }

        const FVector& Min = RenderData.LocalBounds.GetMin();
        const FVector& Max = RenderData.LocalBounds.GetMax();

        const FVector Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f, (Min.z + Max.z) * 0.5f);
        const float ExtentX = (Max.x - Min.x) * 0.5f;
        const float ExtentY = (Max.y - Min.y) * 0.5f;
        const float ExtentZ = (Max.z - Min.z) * 0.5f;
        const float MaxExtent = (std::max)(ExtentX, (std::max)(ExtentY, ExtentZ));
        const float PreviewScale = MaxExtent > 0.0001f ? 1.3f / MaxExtent : 1.0f;

        const FMatrix World = FMatrix::CreateScale(FVector(PreviewScale, PreviewScale, PreviewScale)) * FMatrix::CreateTranslation(FVector(-Center.x, -Center.y, -Center.z));
        const FMatrix View = FMatrix::CreateLookAt(FVector(-4.0f, 0.0f, 0.0f), FVector::Zero, FVector(0.0f, 0.0f, 1.0f));
        const FMatrix Projection = FMatrix::CreatePerspectiveFieldOfView(45.0f * 0.01745329252f, 1.0f, 0.1f, 100.0f);

        UTexture2D* Texture = Material != nullptr ? Material->GetBaseColorTexture() : nullptr;
        ID3D11ShaderResourceView* TextureSRV = Texture != nullptr && Texture->IsValid() ? Texture->GetResource().ShaderResourceView : nullptr;

        Renderer.BeginAssetPreviewRender();
        Renderer.PrepareShader();
        Renderer.UpdateConstant(World, View, Projection, false, TextureSRV != nullptr, true);
        Renderer.RenderPrimitive(RenderData.VertexBuffer, RenderData.IndexBuffer, RenderData.IndexCount, false, TextureSRV);
        Renderer.EndAssetPreviewRender();

        ImGui::Image((ImTextureID)PreviewSRV, ImVec2(192.0f, 192.0f));
    }

    void DrawSelectedAssetPreview(UAsset* Asset, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (Asset == nullptr)
        {
            return;
        }

        ImGui::Separator();
        ImGui::TextDisabled("Preview");

        if (UTexture2D* Texture = Cast<UTexture2D>(Asset))
        {
            if (!Texture->IsValid())
            {
                ImGui::TextDisabled("Texture is unavailable.");
                return;
            }

            const float LongestSide = static_cast<float>((std::max)(Texture->GetSizeX(), Texture->GetSizeY()));
            const float Scale = LongestSide > 0.0f ? 192.0f / LongestSide : 1.0f;
            const ImVec2 PreviewSize(Texture->GetSizeX() * Scale, Texture->GetSizeY() * Scale);

            ImGui::Image((ImTextureID)Texture->GetResource().ShaderResourceView, PreviewSize);
            ImGui::Text("%u x %u", Texture->GetSizeX(), Texture->GetSizeY());
            return;
        }

        if (UStaticMesh* Mesh = Cast<UStaticMesh>(Asset))
        {
            DrawStaticMeshPreview(Renderer, Mesh, nullptr);
            return;
        }

        if (UMaterial* Material = Cast<UMaterial>(Asset))
        {
            TArray<UStaticMesh*> Meshes;
            AssetManager.GetAssets(Meshes);

            UStaticMesh* PreviewMesh = nullptr;

            for (UStaticMesh* Candidate : Meshes)
            {
                if (Candidate != nullptr && std::strcmp(Candidate->GetAssetPath().c_str(), BuiltinAssetPaths::Sphere) == 0)
                {
                    PreviewMesh = Candidate;
                    break;
                }
            }

            if (PreviewMesh == nullptr)
            {
                ImGui::TextDisabled("Preview sphere is unavailable.");
                return;
            }

            DrawStaticMeshPreview(Renderer, PreviewMesh, Material);
        }
    }
}

bool FContentBrowserPanel::Init()
{
    return Context != nullptr && AssetManager != nullptr && Renderer != nullptr;
}

void FContentBrowserPanel::Tick(float DeltaTime)
{
}

void FContentBrowserPanel::OnRender()
{
    if (!IsOpen() || Context == nullptr || AssetManager == nullptr)
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

    static char AssetSearchBuffer[128] = {};

    if (ImGui::Button("Import"))
    {
        ImGui::OpenPopup("ImportAssetMenu");
    }

    if (ImGui::BeginPopup("ImportAssetMenu"))
    {
        if (ImGui::MenuItem("Static Mesh (.obj)"))
        {
            if (UStaticMesh* ImportedAsset = SelectAndImportOBJ(*AssetManager, *Renderer))
            {
                SelectedAsset = ImportedAsset;
                CurrentFolder = GetParentFolderPath(ImportedAsset->GetAssetPath().ToStd());
            }
        }

        if (ImGui::MenuItem("Texture"))
        {
            if (UMaterial* ImportedAsset = SelectAndImportTexture(*AssetManager, *Renderer))
            {
                SelectedAsset = ImportedAsset;
                CurrentFolder = GetParentFolderPath(ImportedAsset->GetAssetPath().ToStd());
            }
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##AssetSearch", "Search Assets...", AssetSearchBuffer, IM_ARRAYSIZE(AssetSearchBuffer));

    const std::string SearchText = AssetSearchBuffer;

    ImGui::Separator();

    TArray<UStaticMesh*> StaticMeshes;
    TArray<UMaterial*> Materials;
    TArray<UTexture2D*> Textures;

    AssetManager->GetAssets(StaticMeshes);
    AssetManager->GetAssets(Materials);
    AssetManager->GetAssets(Textures);

    TArray<UAsset*> AllAssets;

    for (UStaticMesh* Asset : StaticMeshes)
    {
        if (Asset != nullptr)
        {
            AllAssets.Add(Asset);
        }
    }

    for (UMaterial* Asset : Materials)
    {
        if (Asset != nullptr)
        {
            AllAssets.Add(Asset);
        }
    }

    for (UTexture2D* Asset : Textures)
    {
        if (Asset != nullptr)
        {
            AllAssets.Add(Asset);
        }
    }

    AllAssets.Sort([](const UAsset* A, const UAsset* B)
    {
        return A->GetAssetPath() < B->GetAssetPath();
    });

    TArray<std::string> FolderPaths;

    for (UAsset* Asset : AllAssets)
    {
        std::string Folder = GetParentFolderPath(Asset->GetAssetPath().ToStd());

        while (!Folder.empty())
        {
            FolderPaths.AddUnique(Folder);
            Folder = GetParentFolderPath(Folder);
        }
    }

    FolderPaths.Sort();

    if (ImGui::BeginTable("ContentBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn("Sources", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);

        ImGui::BeginChild("##ContentSources", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

        if (ImGui::Selectable("Root", CurrentFolder.empty(), ImGuiSelectableFlags_SpanAvailWidth))
        {
            CurrentFolder.clear();
        }

        DrawContentFolderTree({}, FolderPaths, CurrentFolder);

        ImGui::EndChild();

        ImGui::TableSetColumnIndex(1);

        ImGui::BeginChild("##ContentAssets", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

        if (!CurrentFolder.empty())
        {
            if (ImGui::SmallButton(".."))
            {
                CurrentFolder = GetParentFolderPath(CurrentFolder);
                SelectedAsset = nullptr;
            }

            ImGui::SameLine();
        }

        ImGui::TextUnformatted(CurrentFolder.empty() ? "Root" : CurrentFolder.c_str());

        ImGui::Separator();

        const float AvailableWidth = ImGui::GetContentRegionAvail().x;
        const int32 ColumnCount = (std::max)(1, static_cast<int32>(AvailableWidth / 130.0f));

        if (ImGui::BeginTable("AssetTiles", ColumnCount, ImGuiTableFlags_SizingStretchSame))
        {
            int32 VisibleItemCount = 0;

            for (const std::string& Folder : FolderPaths)
            {
                if (GetParentFolderPath(Folder) != CurrentFolder)
                {
                    continue;
                }

                const std::string FolderName = GetFolderName(Folder);

                if (!SearchText.empty() && Folder.find(SearchText) == std::string::npos)
                {
                    continue;
                }

                ImGui::TableNextColumn();
                ImGui::PushID(Folder.c_str());

                const std::string TileLabel = "[Folder]\n" + FolderName;

                if (ImGui::Selectable(TileLabel.c_str(), false, ImGuiSelectableFlags_None, ImVec2(0.0f, 72.0f)))
                {
                    CurrentFolder = Folder;
                    SelectedAsset = nullptr;
                }

                ImGui::PopID();
                ++VisibleItemCount;
            }

            for (UAsset* Asset : AllAssets)
            {
                if (Asset == nullptr)
                {
                    continue;
                }

                const std::string AssetPath = Asset->GetAssetPath().ToStd();

                if (GetParentFolderPath(AssetPath) != CurrentFolder)
                {
                    continue;
                }

                if (!SearchText.empty() && AssetPath.find(SearchText) == std::string::npos)
                {
                    continue;
                }

                const std::string DisplayName = GetAssetDisplayName(AssetPath);
                const std::string AssetType = GetAssetTypeLabel(Asset);
                const std::string TileLabel = "[" + AssetType + "]\n" + DisplayName;

                ImGui::TableNextColumn();
                ImGui::PushID(Asset);

                const bool bSelected = SelectedAsset == Asset;

                if (ImGui::Selectable(TileLabel.c_str(), bSelected, ImGuiSelectableFlags_None, ImVec2(0.0f, 72.0f)))
                {
                    SelectedAsset = Asset;
                }

                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("%s", AssetPath.c_str());
                }

                ImGui::PopID();
                ++VisibleItemCount;
            }

            ImGui::EndTable();

            if (VisibleItemCount == 0)
            {
                ImGui::TextDisabled("This folder is empty.");
            }
        }

        if (SelectedAsset != nullptr)
        {
            ImGui::Separator();
            ImGui::Text("Selected Asset");
            ImGui::Text("Type: %s", GetAssetTypeLabel(SelectedAsset));
            ImGui::TextWrapped("Path: %s", SelectedAsset->GetAssetPath().c_str());
            DrawSelectedAssetPreview(SelectedAsset, *AssetManager, *Renderer);
        }

        ImGui::EndChild();

        ImGui::EndTable();
    }

    ImGui::End();
    SetOpen(bIsOpen);
}