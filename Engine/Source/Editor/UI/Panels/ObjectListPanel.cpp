#include "ObjectListPanel.h"

#include "imgui.h"

#include "Resources/StaticMesh.h"
#include "Resources/PrimitiveMeshAssets.h"

#include "Components/StaticMeshComponent.h"
#include "Components/CameraComponent.h"
#include "Resources/AssetManager.h"
#include "TextureAssetImporter.h"
#include "Resources/Material.h"
#include "Components/SceneComponent.h"
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

#include "imgui_internal.h"

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
    
    
    void DrawSceneComponentTree(USceneComponent* Component, FEditorObjectContext& Context)
    {
        if (Component == nullptr)
        {
            return;
        }

        const FString ComponentName = Component->GetName();
        const FString Label = ComponentName + "_" + std::to_string(Component->GetUUID());

        const bool bSelected = Context.SelectedComponent == Component;
        const bool bHasChildren = !Component->GetAttachChildren().IsEmpty();

        ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow;

        if (bSelected)
        {
            Flags |= ImGuiTreeNodeFlags_Selected;
        }

        if (!bHasChildren)
        {
            Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        const bool bExpanded = ImGui::TreeNodeEx(Component, Flags, "%s", Label.c_str());

        if (ImGui::IsItemClicked())
        {
            Context.SelectComponent(Component);
        }

        if (bExpanded && bHasChildren)
        {
            for (USceneComponent* Child : Component->GetAttachChildren())
            {
                DrawSceneComponentTree(Child, Context);
            }

            ImGui::TreePop();
        }
    }

}

bool FObjectListPanel::Init()
{
    if (Context == nullptr || AssetManager == nullptr)
    {
        return false;
    }

    return Context != nullptr && AssetManager != nullptr;
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

    
    
    static char ActorSearchBuffer[128] = {};

    AActor* PendingDeleteActor = nullptr;

    bool bPendingCopy = false;
    bool bPendingPaste = false;
    bool bPendingDuplicate = false;
    bool bPendingDeleteSelected = false;

    const bool bCanPaste = Context->UndoManager != nullptr && Context->UndoManager->CanPaste();

    TArray<UStaticMesh*> StaticMeshes;
    AssetManager->GetAssets(StaticMeshes);

    StaticMeshes.Sort([](const UStaticMesh* A, const UStaticMesh* B)
    {
        return A->GetAssetPath() < B->GetAssetPath();
    });

    if (ImGui::Button("+ Add"))
    {
        ImGui::OpenPopup("AddActorMenu");
    }

    if (ImGui::BeginPopup("AddActorMenu"))
    {
        if (ImGui::BeginMenu("Static Mesh Actor"))
        {
            for (UStaticMesh* Mesh : StaticMeshes)
            {
                if (Mesh == nullptr)
                {
                    continue;
                }

                ImGui::PushID(Mesh);

                if (ImGui::MenuItem(Mesh->GetAssetPath().c_str()))
                {
                    SpawnStaticMeshActor(*Context, Mesh);
                }

                ImGui::PopID();
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Camera Actor"))
        {
            SpawnCameraActor(*Context);
        }

        ImGui::EndPopup();
    }

    if (ImGui::GetContentRegionAvail().x > 60.0f) ImGui::SameLine();

    if (ImGui::Button("..."))
    {
        ImGui::OpenPopup("OutlinerActions");
    }

    if (ImGui::BeginPopup("OutlinerActions"))
    {
        const bool bHasSelection = Context->SelectedActor != nullptr;
        const bool bCanCopy = bHasSelection && Context->UndoManager != nullptr;
        const bool bCanDuplicate = bCanCopy;

        ImGui::BeginDisabled(!bCanCopy);
        if (ImGui::MenuItem("Copy"))
        {
            bPendingCopy = true;
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!bCanPaste);
        if (ImGui::MenuItem("Paste"))
        {
            bPendingPaste = true;
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!bCanDuplicate);
        if (ImGui::MenuItem("Duplicate"))
        {
            bPendingDuplicate = true;
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!bHasSelection);
        if (ImGui::MenuItem("Delete"))
        {
            bPendingDeleteSelected = true;
        }
        ImGui::EndDisabled();

        ImGui::EndPopup();
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##ActorSearch", "Search Actors...", ActorSearchBuffer, IM_ARRAYSIZE(ActorSearchBuffer));

    ImGui::Separator();

    ImGui::BeginChild("##ActorList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

    const std::string SearchText = ActorSearchBuffer;
    ULevel* Level = Context->World != nullptr ? Context->World->GetPersistentLevel() : nullptr;

    if (Level != nullptr)
    {
        for (AActor* Actor : Level->GetActors())
        {
            if (Actor == nullptr)
            {
                continue;
            }

            const FString ActorName = Actor->GetName();

            if (!SearchText.empty() && ActorName.ToStd().find(SearchText) == std::string::npos)
            {
                continue;
            }

            ImGui::PushID(Actor);

            if (ImGui::Selectable(ActorName.c_str(), Context->SelectedActor == Actor))
            {
                Context->SelectActor(Actor);
            }

            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", ActorName.c_str());
            }

            if (ImGui::BeginPopupContextItem("ActorContextMenu"))
            {
                Context->SelectActor(Actor);

                if (ImGui::MenuItem("Copy", "Ctrl+C"))
                {
                    bPendingCopy = true;
                }

                if (ImGui::MenuItem("Paste", "Ctrl+V", false, bCanPaste))
                {
                    bPendingPaste = true;
                }

                if (ImGui::MenuItem("Duplicate", "Ctrl+W"))
                {
                    bPendingDuplicate = true;
                }

                ImGui::Separator();

                if (ImGui::MenuItem("Delete", "Delete"))
                {
                    PendingDeleteActor = Actor;
                }

                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
    }

    ImGui::EndChild();

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