#include "ObjectListPanel.h"

#include "imgui.h"

#include "Resources/StaticMesh.h"
#include "Resources/PrimitiveMeshAssets.h"

#include "Components/StaticMeshComponent.h"
#include "Components/CameraComponent.h"
#include "Resources/AssetManager.h"

#include "UObject/Class.h"

namespace
{
    void SpawnStaticMeshObject(FEditorObjectContext& Context, UStaticMesh* InMesh)
    {
        if (InMesh == nullptr)
        {
            return;
        }

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
    }

    void SpawnCameraObject(FEditorObjectContext& Context)
    {
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

    if (ImGui::Button("Camera"))
    {
        SpawnCameraObject(*Context);
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
        ImGui::PopID();
    }
    ImGui::End();
}
