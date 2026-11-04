#include "ObjectListPanel.h"

#include "imgui.h"
#include "Components/CameraComponent.h"
#include "Components/CubeComp.h"
#include "Components/SphereComp.h"
#include "Renderer/Renderer.h"
#include "UObject/Class.h"

template<typename TObject>
static void SpawnEditorObject(FEditorObjectContext& Context, URenderer& Renderer)
{
    TObject* NewInstance = NewObject<TObject>();
    if (NewInstance == nullptr)
    {
        return;
    }

    if (USceneComponent* Scene = Cast<USceneComponent>(NewInstance))
    {
        FTransform Transform = Scene->GetTransform();
        Transform.Location.y += static_cast<float>(Context.Objects.Num()) * 1.5f;
        Scene->SetTransform(Transform);
    }

    FEditorObjectEntry Entry;
    Entry.Object = NewInstance;

    if constexpr (std::is_same_v<TObject, USphereComp> || std::is_same_v<TObject, UCubeComp>)
    {
        TArray<FVertexSimple> Vertices = NewInstance->GetVertex();

        if (!Vertices.IsEmpty())
        {
            Entry.VertexBuffer = Renderer.CreateVertexBuffer(Vertices.GetData(), static_cast<UINT>(sizeof(FVertexSimple) * Vertices.Num()));
            Entry.VertexCount = static_cast<uint32>(Vertices.Num());
        }
    }

    Context.Objects.Add(Entry);
    Context.SelectedObject = NewInstance;
}

bool FObjectListPanel::Init()
{
    return Context != nullptr && Renderer != nullptr;
}

void FObjectListPanel::Tick(float DeltaTime)
{
}

void FObjectListPanel::OnRender()
{
    if (Context == nullptr || Renderer == nullptr)
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
        SpawnEditorObject<USphereComp>(*Context, *Renderer);
    }

    ImGui::SameLine();

    if (ImGui::Button("Cube"))
    {
        SpawnEditorObject<UCubeComp>(*Context, *Renderer);
    }

    ImGui::SameLine();

    if (ImGui::Button("Camera"))
    {
        SpawnEditorObject<UCameraComponent>(*Context, *Renderer);
    }

    ImGui::Separator();
    
    for (const FEditorObjectEntry& Entry : Context->Objects)
    {
        UObject* Object = Entry.Object;
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
