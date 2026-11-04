#include "ObjectListPanel.h"

#include "imgui.h"
#include "Components/CameraComponent.h"
#include "Components/CubeComp.h"
#include "Components/SphereComp.h"
#include "UObject/Class.h"

template<typename TObject>
static void SpawnEditorObject(FEditorObjectContext& Context, const FStaticMeshData* InMeshData)
{
    TObject* NewInstance = NewObject<TObject>();
    if (NewInstance == nullptr)
    {
        return;
    }
    
    if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(NewInstance))
    {
        if (InMeshData == nullptr)
        {
            delete NewInstance;
            return;
        }
        Primitive->SetStaticMeshData(InMeshData);
    }
    if (USceneComponent* Scene = Cast<USceneComponent>(NewInstance))
    {
        FTransform Transform = Scene->GetTransform();
        Transform.Location.y += static_cast<float>(Context.Objects.Num()) * 1.5f;
        Scene->SetTransform(Transform);
    }
    
    Context.Objects.Add(NewInstance);
    Context.SelectedObject = NewInstance;
}

bool FObjectListPanel::Init()
{
    return Context != nullptr && CubeMeshData != nullptr && SphereMeshData != nullptr;
}

void FObjectListPanel::Tick(float DeltaTime)
{
}

void FObjectListPanel::OnRender()
{
    if (Context == nullptr)
    {
        return ;
    }
    
    if (!ImGui::Begin("Object List"))
    {
        ImGui::End();
        return;
    }
    
    if (ImGui::Button("Sphere"))
    {
        SpawnEditorObject<USphereComp>(*Context, SphereMeshData);
    }

    ImGui::SameLine();

    if (ImGui::Button("Cube"))
    {
        SpawnEditorObject<UCubeComp>(*Context, CubeMeshData);
    }

    ImGui::SameLine();

    if (ImGui::Button("Camera"))
    {
        SpawnEditorObject<UCameraComponent>(*Context, nullptr);
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
