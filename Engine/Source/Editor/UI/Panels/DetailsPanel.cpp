#include "DetailsPanel.h"

#include "imgui.h"
#include "UObject/Class.h"
#include "EditorUndoManager.h"
#include "Components/StaticMeshComponent.h"
#include "Resources/AssetManager.h"
#include "Resources/StaticMesh.h"
#include "Resources/Material.h"
#include "Actors/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/ActorComponent.h"

#include <algorithm>

static void DrawStaticMeshSettings(FEditorObjectContext* Context, FAssetManager* AssetManager, UStaticMeshComponent* Component)
{
    if (Context == nullptr || AssetManager == nullptr || Component == nullptr)
    {
        return;
    }

    ImGui::Separator();
    ImGui::Text("Static Mesh");

    TArray<UStaticMesh*> StaticMeshes;
    AssetManager->GetAssets(StaticMeshes);

    StaticMeshes.Sort([](const UStaticMesh* A, const UStaticMesh* B)
    {
        return A->GetAssetPath() < B->GetAssetPath();
    });

    UStaticMesh* CurrentMesh = Component->GetStaticMesh();
    const char* MeshPreview = CurrentMesh != nullptr ? CurrentMesh->GetAssetPath().c_str() : "None";

    if (ImGui::BeginCombo("Static Mesh", MeshPreview))
    {
        for (UStaticMesh* Mesh : StaticMeshes)
        {
            const bool bSelected = CurrentMesh == Mesh;

            if (ImGui::Selectable(Mesh->GetAssetPath().c_str(), bSelected))
            {
                UStaticMesh* PreviousMesh = CurrentMesh;
                Component->SetStaticMesh(Mesh);

                if (Context->UndoManager != nullptr)
                {
                    Context->UndoManager->RecordStaticMeshChange(*Context, Component, PreviousMesh, Mesh);
                }
            }

            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::Text("Material");

    TArray<UMaterial*> Materials;
    AssetManager->GetAssets(Materials);

    Materials.Sort([](const UMaterial* A, const UMaterial* B)
    {
        return A->GetAssetPath() < B->GetAssetPath();
    });

    UMaterial* CurrentMaterial = Component->GetMaterial();
    const char* MaterialPreview = CurrentMaterial != nullptr ? CurrentMaterial->GetAssetPath().c_str() : "None";

    if (ImGui::BeginCombo("Material", MaterialPreview))
    {
        if (ImGui::Selectable("None", CurrentMaterial == nullptr))
        {
            UMaterial* PreviousMaterial = CurrentMaterial;
            Component->SetMaterial(nullptr);

            if (Context->UndoManager != nullptr)
            {
                Context->UndoManager->RecordMaterialChange(*Context, Component, PreviousMaterial, nullptr);
            }
        }

        for (UMaterial* Material : Materials)
        {
            const bool bSelected = CurrentMaterial == Material;

            if (ImGui::Selectable(Material->GetAssetPath().c_str(), bSelected))
            {
                UMaterial* PreviousMaterial = CurrentMaterial;
                Component->SetMaterial(Material);

                if (Context->UndoManager != nullptr)
                {
                    Context->UndoManager->RecordMaterialChange(*Context, Component, PreviousMaterial, Material);
                }
            }

            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }
}

static void DrawProperty(FEditorObjectContext* Context, UObject* Object, const FProperty* Property)
{
    if (Object == nullptr || Property == nullptr)
    {
        return;
    }
    
    if (!Property->IsVisible())
    {
        return;
    }
    
    void* ValueAddress = Property->GetValuePtr(Object);
    
    if (ValueAddress == nullptr)
    {
        return;
    }
    const bool bEditable = Property->IsEditable();
    
    const FString Label = Property->GetDisplayName().IsEmpty() ? Property->GetName() : Property->GetDisplayName();
    
    auto CommitValue = [&](bool bChanged, const void* NewValue)
    {
        if (bChanged && bEditable)
        {
            if (Context != nullptr && Context->UndoManager != nullptr)
            {
                Context->UndoManager->BeginPropertyEdit(*Context, Object, Property);
            }

            Property->SetValue(Object, Property->GetType(), NewValue);
        }

        if (bEditable && Context != nullptr && Context->UndoManager != nullptr && ImGui::IsItemDeactivatedAfterEdit())
        {
            Context->UndoManager->EndPropertyEdit(*Context, Object, Property);
        }
    };
    
    ImGui::PushID(Property);
    ImGui::BeginDisabled(!bEditable);
    
    switch (Property->GetType())
    {
    case EPropertyType::Bool:
    {
        bool Value = *static_cast<bool*>(ValueAddress);

        bool bChanged = ImGui::Checkbox(Label.c_str(), &Value);

        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::Int32:
    {
        int32 Value = *static_cast<int32*>(ValueAddress);

        bool bChanged = ImGui::InputScalar(Label.c_str(), ImGuiDataType_S32, &Value);

        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::UInt32:
    {
        uint32 Value = *static_cast<uint32*>(ValueAddress);

        bool bChanged = ImGui::InputScalar(Label.c_str(), ImGuiDataType_U32, &Value);

        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::Float:
    {
        float Value = *static_cast<float*>(ValueAddress);

        bool bChanged = ImGui::DragFloat(Label.c_str(), &Value, 0.1f);

        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::Double:
    {
        double Value = *static_cast<double*>(ValueAddress);

        bool bChanged = ImGui::InputDouble(Label.c_str(), &Value);

        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::String:
    {
        const FString& CurrentValue = *static_cast<FString*>(ValueAddress);

        char Buffer[256] = {};

        SIZE_T CopySize = CurrentValue.size();

        if (CopySize >= sizeof(Buffer))
        {
            CopySize = sizeof(Buffer) - 1;
        }

        std::memcpy(Buffer, CurrentValue.c_str(), CopySize);

        const bool bChanged = ImGui::InputText(Label.c_str(), Buffer, sizeof(Buffer));
        FString NewValue(Buffer);
        CommitValue(bChanged, &NewValue);

        break;
    }

    case EPropertyType::Vector:
    {
        FVector Value = *static_cast<FVector*>(ValueAddress);

        bool bChanged = ImGui::DragFloat3(Label.c_str(), &Value.x, 0.1f);

        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::Rotator:
    {
        const FRotator& CurrentValue = *static_cast<FRotator*>(ValueAddress);

        float Values[3] = {CurrentValue.Pitch, CurrentValue.Yaw, CurrentValue.Roll};

        const bool bChanged = ImGui::DragFloat3(Label.c_str(), Values, 0.1f);
        const FRotator NewValue(Values[0], Values[1], Values[2]);
        CommitValue(bChanged, &NewValue);
        break;
    }

    case EPropertyType::Transform:
    {
        FTransform Value = *static_cast<FTransform*>(ValueAddress);

        if (ImGui::TreeNode(Label.c_str()))
        {
            const bool bLocationChanged = ImGui::DragFloat3("Location", &Value.Location.x, 0.1f);
            CommitValue(bLocationChanged, &Value);

            float Rotation[3] = {Value.Rotation.Pitch, Value.Rotation.Yaw, Value.Rotation.Roll};
            const bool bRotationChanged = ImGui::DragFloat3("Rotation", Rotation, 0.1f);

            if (bRotationChanged)
            {
                Value.Rotation = FRotator(Rotation[0], Rotation[1], Rotation[2]);
            }

            CommitValue(bRotationChanged, &Value);

            const bool bScaleChanged = ImGui::DragFloat3("Scale", &Value.Scale3D.x, 0.1f);
            CommitValue(bScaleChanged, &Value);
            ImGui::TreePop();
        }
        break;
    }
    default:
    {
        ImGui::TextDisabled("%s (Unsupported Property Type)", Label.c_str());
        break;
    }
    }
    ImGui::EndDisabled();
    ImGui::PopID();
}

bool FDetailsPanel::Init()
{
    return Context != nullptr;
}

void FDetailsPanel::Tick(float DeltaTime)
{
}

void FDetailsPanel::OnRender()
{
    if (!ImGui::Begin("Details"))
    {
        ImGui::End();
        return;
    }
    if (Context == nullptr || Context->SelectedActor == nullptr)
    {
        ImGui::TextDisabled("No Actor selected.");
        ImGui::End();
        return;
    }

    AActor* SelectedActor = Context->SelectedActor;
    UObject* SelectedObject = Context->SelectedComponent != nullptr ? Context->SelectedComponent : SelectedActor->GetRootComponent();

    ImGui::Text("Actor: %s", SelectedActor->GetName().c_str());
    ImGui::Text("Actor UUID: %u", SelectedActor->GetUUID());

    if (SelectedObject == nullptr)
    {
        ImGui::TextDisabled("No Component selected.");
        ImGui::End();
        return;
    }

    UClass* ObjectClass = SelectedObject->GetClass();
    if (ObjectClass == nullptr)
    {
        ImGui::End();
        return;
    }
    ImGui::Text("%s", ObjectClass->GetName().c_str());
    
    ImGui::Text("UUID: %u", SelectedObject->GetUUID());

    if (UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(SelectedObject))
    {
        DrawStaticMeshSettings(Context, AssetManager, Component);
    }

    ImGui::Separator();
    
    TArray<const FProperty*> Properties;
    ObjectClass->GetProperties(Properties, true);

    TArray<FString> Categories;

    for (const FProperty* Property : Properties)
    {
        if (!Property->IsVisible())
        {
            continue;
        }

        const FString& Category = Property->GetCategory();

        if (!Categories.Contains(Category))
        {
            Categories.Add(Category);
        }
    }

    for (const FString& Category : Categories)
    {
        if (!ImGui::CollapsingHeader(Category.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            continue;
        }

        for (const FProperty* Property : Properties)
        {
            if (Property->GetCategory() == Category)
            {
                DrawProperty(Context, SelectedObject, Property);
            }
        }
    }
    ImGui::End();
}
