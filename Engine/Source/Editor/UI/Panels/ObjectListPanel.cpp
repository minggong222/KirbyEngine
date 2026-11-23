#include "ObjectListPanel.h"

#include "imgui.h"

#include "Resources/StaticMesh.h"
#include "Resources/PrimitiveMeshAssets.h"

#include "Components/StaticMeshComponent.h"
#include "Components/CameraComponent.h"
#include "Resources/AssetManager.h"

#include "UObject/Class.h"
#include "ViewportPanel.h"
#include "Core/Math/MathUtility.h"
#include "UObject/Object.h"
#include "EditorUndoManager.h"
#include "OBJStaticMeshImporter.h"

#include <Windows.h>
#include <commdlg.h>
#include <filesystem>

#pragma comment(lib, "Comdlg32.lib")

namespace
{
    void SpawnStaticMeshObject(FEditorObjectContext& Context, UStaticMesh* InMesh)
    {
        if (InMesh == nullptr)
        {
            return;
        }

        const uint32 PreviousSelectedUUID = Context.SelectedObject != nullptr ? Context.SelectedObject->GetUUID() : 0;
        UStaticMeshComponent* NewInstance = NewObject<UStaticMeshComponent>();

        if (NewInstance == nullptr)
        {
            return;
        }

        NewInstance->SetStaticMesh(InMesh);
        FTransform Transform = NewInstance->GetTransform();
        Transform.Location.y += static_cast<float>(Context.Objects.Num()) * 1.5f;
        NewInstance->SetTransform(Transform);
        Context.Objects.Add(NewInstance);
        Context.SelectedObject = NewInstance;

        if (Context.UndoManager != nullptr)
        {
            Context.UndoManager->RecordCreate(Context, NewInstance, PreviousSelectedUUID);
        }
    }

    void SpawnCameraObject(FEditorObjectContext& Context)
    {
        const uint32 PreviousSelectedUUID = Context.SelectedObject != nullptr ? Context.SelectedObject->GetUUID() : 0;
        UCameraComponent* NewInstance = NewObject<UCameraComponent>();

        if (NewInstance == nullptr)
        {
            return;
        }

        FTransform Transform = NewInstance->GetTransform();
        Transform.Location.y += static_cast<float>(Context.Objects.Num()) * 1.5f;
        NewInstance->SetTransform(Transform);
        Context.Objects.Add(NewInstance);
        Context.SelectedObject = NewInstance;
        
        if (Context.UndoManager != nullptr)
        {
            Context.UndoManager->RecordCreate(Context, NewInstance, PreviousSelectedUUID);
        }
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
    
    bool DeleteObject(FEditorObjectContext& Context, FViewportPanel* ViewportPanel, UObject* TargetObject)
    {
        if (TargetObject == nullptr)
        {
            return false;
        }

        const int32 ObjectIndex = Context.Objects.Find(TargetObject);

        if (ObjectIndex == INDEX_NONE)
        {
            return false;
        }

        if (ViewportPanel != nullptr)
        {
            ViewportPanel->CancelGizmoDrag();
        }
        
        if (Context.UndoManager != nullptr)
        {
            Context.UndoManager->RecordDelete(Context, TargetObject);
        }

        if (Context.SelectedObject == TargetObject)
        {
            Context.SelectedObject = nullptr;
        }


        Context.Objects.RemoveAt(ObjectIndex);

        delete TargetObject;

        return true;
    }
    
    UObject* DuplicateObject(FEditorObjectContext& Context, FViewportPanel* ViewportPanel)
    {
        UObject* SourceObject = Context.SelectedObject;

        if (SourceObject == nullptr)
        {
            return nullptr;
        }

        const uint32 PreviousSelectedUUID = SourceObject->GetUUID();

        if (ViewportPanel != nullptr)
        {
            ViewportPanel->CancelGizmoDrag();
        }
        
        UObject* NewObjectInstance = nullptr;

        if (UStaticMeshComponent* SourceComponent = Cast<UStaticMeshComponent>(SourceObject))
        {
            UStaticMeshComponent* NewComponent = NewObject<UStaticMeshComponent>();

            if (NewComponent == nullptr)
            {
                return nullptr;
            }

            NewComponent->SetStaticMesh(SourceComponent->GetStaticMesh());

            FTransform NewTransform = SourceComponent->GetWorldTransform();
            NewTransform.Location.y += 1.5f;

            NewComponent->SetTransform(NewTransform);
            NewObjectInstance = NewComponent;
        }
        else if (UCameraComponent* SourceCamera = Cast<UCameraComponent>(SourceObject))
        {
            UCameraComponent* NewCamera = NewObject<UCameraComponent>();

            if (NewCamera == nullptr)
            {
                return nullptr;
            }

            NewCamera->SetTransform(SourceCamera->GetWorldTransform());

            if (!CopyCameraProperties(*SourceCamera, *NewCamera))
            {
                delete NewCamera;
                return nullptr;
            }

            FTransform NewTransform = NewCamera->GetTransform();
            NewTransform.Location.y += 1.5f;

            NewCamera->SetTransform(NewTransform);
            NewObjectInstance = NewCamera;
        }

        if (NewObjectInstance == nullptr)
        {
            return nullptr;
        }

        Context.Objects.Add(NewObjectInstance);
        Context.SelectedObject = NewObjectInstance;

        if (Context.UndoManager != nullptr)
        {
            Context.UndoManager->RecordCreate(Context, NewObjectInstance, PreviousSelectedUUID);
        }
        
        return NewObjectInstance;
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
}

bool FObjectListPanel::Init()
{
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

    if (!ImGui::Begin("Object List"))
    {
        ImGui::End();
        return;
    }

    UObject* PendingDeleteObject = nullptr;
    bool bPendingCopy = false;
    bool bPendingPaste = false;
    bool bPendingDuplicate = false;
    bool bPendingDeleteSelected = false;

    if (ImGui::Button("Sphere"))
    {
        UStaticMesh* SphereMesh = AssetManager->Find<UStaticMesh>(BuiltinAssetPaths::Sphere);
        SpawnStaticMeshObject(*Context, SphereMesh);
    }

    ImGui::SameLine();

    if (ImGui::Button("Cube"))
    {
        UStaticMesh* CubeMesh = AssetManager->Find<UStaticMesh>(BuiltinAssetPaths::Cube);
        SpawnStaticMeshObject(*Context, CubeMesh);
    }

    ImGui::SameLine();

    if (ImGui::Button("Plane"))
    {
        UStaticMesh* PlaneMesh = AssetManager->Find<UStaticMesh>(BuiltinAssetPaths::Plane);
        SpawnStaticMeshObject(*Context, PlaneMesh);
    }

    ImGui::SameLine();

    if (ImGui::Button("Camera"))
    {
        SpawnCameraObject(*Context);
    }

    ImGui::SameLine();

    if (ImGui::Button("Import OBJ") && Renderer != nullptr)
    {
        UStaticMesh* ImportedMesh = SelectAndImportOBJ(*AssetManager, *Renderer);
        SpawnStaticMeshObject(*Context, ImportedMesh);
    }
    
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
    
    for (UObject* Object : Context->Objects)
    {
        if (Object == nullptr)
        {
            continue;
        }

        UClass* ObjectClass = Object->GetClass();

        if (ObjectClass == nullptr)
        {
            continue;
        }

        char ObjectLabel[128] = {};
        std::snprintf(ObjectLabel, sizeof(ObjectLabel), "%s_%u", ObjectClass->GetName().c_str(), Object->GetUUID());

        ImGui::PushID(Object);

        if (ImGui::Selectable(ObjectLabel, Context->SelectedObject == Object))
        {
            Context->SelectedObject = Object;
        }

        if (ImGui::BeginPopupContextItem("ObjectContextMenu"))
        {
            Context->SelectedObject = Object;

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
                PendingDeleteObject = Object;
            }

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::End();

    if (PendingDeleteObject != nullptr)
    {
        DeleteObject(*Context, ViewportPanel, PendingDeleteObject);
    }
    else if (bPendingDeleteSelected)
    {
        DeleteObject(*Context, ViewportPanel, Context->SelectedObject);
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
        DuplicateObject(*Context, ViewportPanel);
    }
}

void FObjectListPanel::DeleteSelectedObject()
{
    if (Context == nullptr || Context->SelectedObject == nullptr)
    {
        return;
    }

    DeleteObject(*Context, ViewportPanel, Context->SelectedObject);
}

void FObjectListPanel::DuplicateSelectedObject()
{
    if (Context == nullptr || Context->SelectedObject == nullptr)
    {
        return;
    }

    DuplicateObject(*Context, ViewportPanel);
}