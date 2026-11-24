#include "ObjectListPanel.h"

#include "imgui.h"

#include "Resources/StaticMesh.h"
#include "Resources/PrimitiveMeshAssets.h"

#include "Components/StaticMeshComponent.h"
#include "Components/CameraComponent.h"
#include "Resources/AssetManager.h"
#include "TextureAssetImporter.h"
#include "Resources/Material.h"

#include "UObject/Class.h"
#include "ViewportPanel.h"
#include "Core/Math/MathUtility.h"
#include "UObject/Object.h"
#include "EditorUndoManager.h"
#include "OBJStaticMeshImporter.h"
#include "Actors/Actor.h"
#include "Actors/StaticMeshActor.h"
#include "Actors/CameraActor.h"
#include "World/World.h"
#include "World/Level.h"
#include <Windows.h>
#include <commdlg.h>
#include <filesystem>

#pragma comment(lib, "Comdlg32.lib")

namespace
{
    AStaticMeshActor* SpawnStaticMeshActor(FEditorObjectContext& Context, UStaticMesh* InMesh)
    {
        if (Context.World == nullptr || InMesh == nullptr)
        {
            return nullptr;
        }

        ULevel* Level = Context.World->GetPersistentLevel();
        const uint32 PreviousSelectedUUID = Context.GetSelectedObject() != nullptr ? Context.GetSelectedObject()->GetUUID() : 0;

        FTransform Transform;
        Transform.Location.y = Level != nullptr ? static_cast<float>(Level->GetActors().Num()) * 1.5f : 0.0f;

        AStaticMeshActor* NewActor = Context.World->SpawnActor<AStaticMeshActor>(FName("StaticMeshActor"), Transform);

        if (NewActor == nullptr || NewActor->GetStaticMeshComponent() == nullptr)
        {
            if (NewActor != nullptr)
            {
                Context.World->DestroyActor(NewActor);
            }

            return nullptr;
        }

        NewActor->GetStaticMeshComponent()->SetStaticMesh(InMesh);
        Context.SelectActor(NewActor);

        if (Context.UndoManager != nullptr)
        {
            Context.UndoManager->RecordCreate(Context, NewActor, PreviousSelectedUUID);
        }

        return NewActor;
    }

    ACameraActor* SpawnCameraActor(FEditorObjectContext& Context)
    {
        if (Context.World == nullptr)
        {
            return nullptr;
        }

        ULevel* Level = Context.World->GetPersistentLevel();
        const uint32 PreviousSelectedUUID = Context.GetSelectedObject() != nullptr ? Context.GetSelectedObject()->GetUUID() : 0;

        FTransform Transform;
        Transform.Location.y = Level != nullptr ? static_cast<float>(Level->GetActors().Num()) * 1.5f : 0.0f;

        ACameraActor* NewActor = Context.World->SpawnActor<ACameraActor>(FName("CameraActor"), Transform);

        if (NewActor == nullptr)
        {
            return nullptr;
        }

        Context.SelectActor(NewActor);

        if (Context.UndoManager != nullptr)
        {
            Context.UndoManager->RecordCreate(Context, NewActor, PreviousSelectedUUID);
        }

        return NewActor;
    }
    
    bool CopyCameraProperties(const UCameraComponent& Source, UCameraComponent& Destination)
    {
        TArray<const FProperty*> Properties;
        Source.GetClass()->GetProperties(Properties, true);

        for (const FProperty* Property : Properties)
        {
            if (Property->GetName().ToStd() == "Transform")
            {
                continue;
            }

            const void* SourceValue = Property->GetValuePtr(&Source);

            if (SourceValue == nullptr || !Property->SetValue(&Destination, Property->GetType(), SourceValue))
            {
                return false;
            }
        }

        return true;
    }
    
    bool DeleteActor(FEditorObjectContext& Context, FViewportPanel* ViewportPanel, AActor* TargetActor)
    {
        if (Context.World == nullptr || TargetActor == nullptr || TargetActor->GetLevel() != Context.World->GetPersistentLevel())
        {
            return false;
        }

        if (ViewportPanel != nullptr)
        {
            ViewportPanel->CancelGizmoDrag();
        }

        if (Context.UndoManager != nullptr)
        {
            Context.UndoManager->RecordDelete(Context, TargetActor);
        }

        if (Context.SelectedActor == TargetActor)
        {
            Context.ClearSelection();
        }

        return Context.World->DestroyActor(TargetActor);
    }
    
    AActor* DuplicateActor(FEditorObjectContext& Context, FViewportPanel* ViewportPanel)
    {
        if (Context.SelectedActor == nullptr || Context.UndoManager == nullptr)
        {
            return nullptr;
        }

        if (ViewportPanel != nullptr)
        {
            ViewportPanel->CancelGizmoDrag();
        }

        if (!Context.UndoManager->CopySelected(Context))
        {
            return nullptr;
        }

        return Cast<AActor>(Context.UndoManager->PasteCopied(Context));
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
}

bool FObjectListPanel::Init()
{
    if (Context == nullptr || AssetManager == nullptr)
    {
        return false;
    }

    MeshToSpawn = AssetManager->Find<UStaticMesh>(BuiltinAssetPaths::Cube);

    return MeshToSpawn != nullptr;
}

void FObjectListPanel::Tick(float DeltaTime)
{
}

void FObjectListPanel::OnRender()
{
    if (Context == nullptr || AssetManager == nullptr)
    {
        return;
    }

    if (!IsOpen())
    {
        return;
    }

    bool bIsOpen = IsOpen();

    if (!ImGui::Begin("World Outliner", &bIsOpen))
    {
        SetOpen(bIsOpen);
        ImGui::End();
        return;
    }

    AActor* PendingDeleteActor = nullptr;
    
    TArray<UStaticMesh*> StaticMeshes;
    AssetManager->GetAssets(StaticMeshes);

    StaticMeshes.Sort([](const UStaticMesh* A, const UStaticMesh* B)
    {
        return A->GetAssetPath() < B->GetAssetPath();
    });

    if (!StaticMeshes.Contains(MeshToSpawn))
    {
        MeshToSpawn = StaticMeshes.IsEmpty() ? nullptr : StaticMeshes[0];
    }

    const char* MeshPreview = MeshToSpawn != nullptr ? MeshToSpawn->GetAssetPath().c_str() : "No Static Mesh";

    if (ImGui::BeginCombo("Static Mesh", MeshPreview))
    {
        for (UStaticMesh* Mesh : StaticMeshes)
        {
            const bool bSelected = MeshToSpawn == Mesh;

            if (ImGui::Selectable(Mesh->GetAssetPath().c_str(), bSelected))
            {
                MeshToSpawn = Mesh;
            }

            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::SameLine();

    ImGui::BeginDisabled(MeshToSpawn == nullptr);

    if (ImGui::Button("Add Static Mesh"))
    {
        SpawnStaticMeshActor(*Context, MeshToSpawn);
    }

    ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("Camera"))
    {
        SpawnCameraActor(*Context);
    }

    ImGui::SameLine();

    if (ImGui::Button("Import OBJ") && Renderer != nullptr)
    {
        UStaticMesh* ImportedMesh = SelectAndImportOBJ(*AssetManager, *Renderer);

        if (ImportedMesh != nullptr)
        {
            MeshToSpawn = ImportedMesh;
        }
    }

    ImGui::SameLine();

    if (ImGui::Button("Import Texture") && Renderer != nullptr)
    {
        SelectAndImportTexture(*AssetManager, *Renderer);
    }
    
    bool bPendingCopy = false;
    bool bPendingPaste = false;
    bool bPendingDuplicate = false;
    bool bPendingDeleteSelected = false;
    
    if (ImGui::Button("Copy Selected"))
    {
        bPendingCopy = true;
    }

    ImGui::SameLine();

    const bool bCanPaste = Context->UndoManager != nullptr && Context->UndoManager->CanPaste();

    ImGui::BeginDisabled(!bCanPaste);

    if (ImGui::Button("Paste"))
    {
        bPendingPaste = true;
    }

    ImGui::EndDisabled();

    ImGui::SameLine();

    if (ImGui::Button("Duplicate Selected"))
    {
        bPendingDuplicate = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Delete Selected"))
    {
        bPendingDeleteSelected = true;
    }

    ImGui::Separator();
    
    ULevel* Level = Context->World != nullptr ? Context->World->GetPersistentLevel() : nullptr;

    if (Level != nullptr)
    {
        for (AActor* Actor : Level->GetActors())
        {
            if (Actor == nullptr)
            {
                continue;
            }

            ImGui::PushID(Actor);

            const FString ActorName = Actor->GetName();

            if (ImGui::Selectable(ActorName.c_str(), Context->SelectedActor == Actor))
            {
                Context->SelectActor(Actor);
            }

            if (ImGui::BeginPopupContextItem("ActorContextMenu"))
            {
                Context->SelectActor(Actor);

                if (ImGui::MenuItem("Copy", "Ctrl+C"))
                {
                    bPendingCopy = true;
                }

                if (ImGui::MenuItem("Duplicate", "Ctrl+W"))
                {
                    bPendingDuplicate = true;
                }

                if (ImGui::MenuItem("Delete", "Delete"))
                {
                    PendingDeleteActor = Actor;
                }

                ImGui::EndPopup();
            }

            if (ImGui::TreeNode("Components"))
            {
                for (UActorComponent* Component : Actor->GetComponents())
                {
                    if (Component == nullptr)
                    {
                        continue;
                    }

                    FString ComponentLabel = Component->GetClass()->GetName();

                    if (Component == Actor->GetRootComponent())
                    {
                        ComponentLabel.Append(" (Root)");
                    }

                    ComponentLabel.Append("_");
                    ComponentLabel.Append(std::to_string(Component->GetUUID()));

                    if (ImGui::Selectable(ComponentLabel.c_str(), Context->SelectedComponent == Component))
                    {
                        Context->SelectComponent(Component);
                    }
                }

                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }

    ImGui::End();
    SetOpen(bIsOpen);

    if (PendingDeleteActor != nullptr)
    {
        DeleteActor(*Context, ViewportPanel, PendingDeleteActor);
    }
    else if (bPendingDeleteSelected && Context->SelectedActor != nullptr)
    {
        DeleteActor(*Context, ViewportPanel, Context->SelectedActor);
    }
    else if (bPendingCopy)
    {
        if (Context->UndoManager != nullptr)
        {
            Context->UndoManager->CopySelected(*Context);
        }
    }
    else if (bPendingPaste)
    {
        if (Context->UndoManager != nullptr)
        {
            Context->UndoManager->PasteCopied(*Context);
        }
    }
    else if (bPendingDuplicate)
    {
        DuplicateActor(*Context, ViewportPanel);
    }
}

void FObjectListPanel::DeleteSelectedObject()
{
    if (Context == nullptr || Context->SelectedActor == nullptr)
    {
        return;
    }

    DeleteActor(*Context, ViewportPanel, Context->SelectedActor);
}

void FObjectListPanel::DuplicateSelectedObject()
{
    if (Context == nullptr || Context->SelectedActor == nullptr)
    {
        return;
    }

    DuplicateActor(*Context, ViewportPanel);
}