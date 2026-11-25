#include "DetailsPanel.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "UObject/Class.h"
#include "EditorUndoManager.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ActorComponent.h"
#include "Components/CameraComponent.h"

#include "Resources/AssetManager.h"
#include "Resources/StaticMesh.h"
#include "Resources/Material.h"

#include "Actors/Actor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

static void DrawComponentHierarchy(USceneComponent* Component, FEditorObjectContext& Context)
{
    if (Component == nullptr)
    {
        return;
    }

    const bool bSelected = Context.SelectedComponent == Component || (Context.SelectedComponent == nullptr && Context.SelectedActor != nullptr && Context.SelectedActor->GetRootComponent() == Component);
    const bool bHasChildren = !Component->GetAttachChildren().IsEmpty();

    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (bSelected)
    {
        Flags |= ImGuiTreeNodeFlags_Selected;
    }

    if (!bHasChildren)
    {
        Flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    if (Context.SelectedActor != nullptr && Component == Context.SelectedActor->GetRootComponent())
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }

    const bool bExpanded = ImGui::TreeNodeEx(Component, Flags, "%s", Component->GetName().c_str());

    if (ImGui::IsItemClicked())
    {
        Context.SelectComponent(Component);
    }

    if (bExpanded && bHasChildren)
    {
        for (USceneComponent* Child : Component->GetAttachChildren())
        {
            DrawComponentHierarchy(Child, Context);
        }

        ImGui::TreePop();
    }
}

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
            if (Mesh == nullptr)
            {
                continue;
            }

            const bool bSelected = CurrentMesh == Mesh;

            if (ImGui::Selectable(Mesh->GetAssetPath().c_str(), bSelected))
            {
                UStaticMesh* PreviousMesh = CurrentMesh;

                const TArray<UMaterial*> PreviousOverrides = Component->GetMaterialOverrides();
                const TArray<uint8> PreviousOverrideMask = Component->GetMaterialOverrideMask();

                Component->SetStaticMesh(Mesh);

                if (Context->UndoManager != nullptr)
                {
                    Context->UndoManager->RecordStaticMeshChange(*Context, Component, PreviousMesh, Mesh, PreviousOverrides, PreviousOverrideMask, Component->GetMaterialOverrides(), Component->GetMaterialOverrideMask());
                }

                CurrentMesh = Mesh;
            }

            if (bSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }

        ImGui::EndCombo();
    }

    ImGui::Separator();
    ImGui::Text("Material Slots");

    TArray<UMaterial*> Materials;
    AssetManager->GetAssets(Materials);

    Materials.Sort([](const UMaterial* A, const UMaterial* B)
    {
        return A->GetAssetPath() < B->GetAssetPath();
    });

    const int32 MaterialCount = Component->GetMaterialCount();

    if (MaterialCount <= 0)
    {
        ImGui::TextDisabled("This mesh has no material slots.");
        return;
    }

    for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
    {
        ImGui::PushID(MaterialIndex);

        const bool bHasOverride = Component->HasMaterialOverride(MaterialIndex);
        UMaterial* CurrentMaterial = Component->GetMaterial(MaterialIndex);

        const char* MaterialPreview = nullptr;

        if (!bHasOverride)
        {
            MaterialPreview = "Use Mesh Default";
        }
        else if (CurrentMaterial == nullptr)
        {
            MaterialPreview = "None";
        }
        else
        {
            MaterialPreview = CurrentMaterial->GetAssetPath().c_str();
        }

        char SlotLabel[64] = {};
        std::snprintf(SlotLabel, sizeof(SlotLabel), "Material Slot %d", MaterialIndex);

        ImGui::TextUnformatted(SlotLabel);

        if (ImGui::BeginCombo("##MaterialSlot", MaterialPreview))
        {
            if (ImGui::Selectable("Use Mesh Default", !bHasOverride))
            {
                if (bHasOverride)
                {
                    UMaterial* PreviousMaterial = CurrentMaterial;
                    const bool bPreviousOverride = true;

                    Component->ClearMaterialOverride(MaterialIndex);

                    if (Context->UndoManager != nullptr)
                    {
                        Context->UndoManager->RecordMaterialChange(*Context, Component, MaterialIndex, PreviousMaterial, bPreviousOverride, Component->GetMaterial(MaterialIndex), false);
                    }
                }
            }

            if (ImGui::Selectable("None", bHasOverride && CurrentMaterial == nullptr))
            {
                UMaterial* PreviousMaterial = CurrentMaterial;
                const bool bPreviousOverride = bHasOverride;

                Component->SetMaterial(MaterialIndex, nullptr);

                if (Context->UndoManager != nullptr)
                {
                    Context->UndoManager->RecordMaterialChange(*Context, Component, MaterialIndex, PreviousMaterial, bPreviousOverride, nullptr, true);
                }
            }

            ImGui::Separator();

            for (UMaterial* Material : Materials)
            {
                if (Material == nullptr)
                {
                    continue;
                }

                const bool bSelected = bHasOverride && CurrentMaterial == Material;

                if (ImGui::Selectable(Material->GetAssetPath().c_str(), bSelected))
                {
                    UMaterial* PreviousMaterial = CurrentMaterial;
                    const bool bPreviousOverride = bHasOverride;

                    Component->SetMaterial(MaterialIndex, Material);

                    if (Context->UndoManager != nullptr)
                    {
                        Context->UndoManager->RecordMaterialChange(*Context, Component, MaterialIndex, PreviousMaterial, bPreviousOverride, Material, true);
                    }
                }

                if (bSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::PopID();
    }
}

static bool IsValidPropertyInput(UObject* Object, const FProperty* Property, const void* ValuePtr)
{
    if (Object == nullptr || Property == nullptr || ValuePtr == nullptr)
    {
        return false;
    }

    switch (Property->GetType())
    {
    case EPropertyType::Float:
    {
        const float Value = *static_cast<const float*>(ValuePtr);

        if (!std::isfinite(Value))
        {
            return false;
        }

        if (Cast<UCameraComponent>(Object) != nullptr)
        {
            const std::string PropertyName = Property->GetName().ToStd();

            if (PropertyName == "FieldOfView" && (Value <= 1.0f || Value >= 179.0f))
            {
                return false;
            }

            if ((PropertyName == "AspectRatio" || PropertyName == "NearClipPlane" || PropertyName == "FarClipPlane" || PropertyName == "OrthoWidth") && Value <= 0.0f)
            {
                return false;
            }

            if ((PropertyName == "MoveSpeed" || PropertyName == "MouseSensitivity" || PropertyName == "WheelSpeed") && Value < 0.0f)
            {
                return false;
            }

            if (PropertyName == "NearClipPlane" || PropertyName == "FarClipPlane")
            {
                const FProperty* NearProperty = Object->GetClass()->FindProperty("NearClipPlane");
                const FProperty* FarProperty = Object->GetClass()->FindProperty("FarClipPlane");

                if (NearProperty == nullptr || FarProperty == nullptr)
                {
                    return false;
                }

                const void* NearValuePtr = NearProperty->GetValuePtr(Object);
                const void* FarValuePtr = FarProperty->GetValuePtr(Object);

                if (NearValuePtr == nullptr || FarValuePtr == nullptr)
                {
                    return false;
                }

                float NearValue = *static_cast<const float*>(NearValuePtr);
                float FarValue = *static_cast<const float*>(FarValuePtr);

                if (PropertyName == "NearClipPlane")
                {
                    NearValue = Value;
                }
                else
                {
                    FarValue = Value;
                }

                if (FarValue <= NearValue)
                {
                    return false;
                }
            }
        }

        return true;
    }

    case EPropertyType::Double:
        return std::isfinite(*static_cast<const double*>(ValuePtr));

    case EPropertyType::Vector:
    {
        const FVector& Value = *static_cast<const FVector*>(ValuePtr);
        return std::isfinite(Value.x) && std::isfinite(Value.y) && std::isfinite(Value.z);
    }

    case EPropertyType::Rotator:
    {
        const FRotator& Value = *static_cast<const FRotator*>(ValuePtr);
        return std::isfinite(Value.Pitch) && std::isfinite(Value.Yaw) && std::isfinite(Value.Roll);
    }

    case EPropertyType::Transform:
    {
        const FTransform& Value = *static_cast<const FTransform*>(ValuePtr);

        return
            std::isfinite(Value.Location.x) &&
            std::isfinite(Value.Location.y) &&
            std::isfinite(Value.Location.z) &&
            std::isfinite(Value.Rotation.Pitch) &&
            std::isfinite(Value.Rotation.Yaw) &&
            std::isfinite(Value.Rotation.Roll) &&
            std::isfinite(Value.Scale3D.x) &&
            std::isfinite(Value.Scale3D.y) &&
            std::isfinite(Value.Scale3D.z) &&
            std::fabs(Value.Scale3D.x) > EPSILON_1E_6 &&
            std::fabs(Value.Scale3D.y) > EPSILON_1E_6 &&
            std::fabs(Value.Scale3D.z) > EPSILON_1E_6;
    }

    default:
        return true;
    }
}

static void DrawProperty(FEditorObjectContext* Context, UObject* Object, const FProperty* Property)
{
    if (Object == nullptr || Property == nullptr || !Property->IsVisible())
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
        if (bChanged && bEditable && IsValidPropertyInput(Object, Property, NewValue))
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
        const bool bChanged = ImGui::Checkbox(Label.c_str(), &Value);
        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::Int32:
    {
        int32 Value = *static_cast<int32*>(ValueAddress);
        const bool bChanged = ImGui::InputScalar(Label.c_str(), ImGuiDataType_S32, &Value);
        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::UInt32:
    {
        uint32 Value = *static_cast<uint32*>(ValueAddress);
        const bool bChanged = ImGui::InputScalar(Label.c_str(), ImGuiDataType_U32, &Value);
        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::Float:
    {
        float Value = *static_cast<float*>(ValueAddress);
        const bool bChanged = ImGui::DragFloat(Label.c_str(), &Value, 0.1f);
        CommitValue(bChanged, &Value);
        break;
    }

    case EPropertyType::Double:
    {
        double Value = *static_cast<double*>(ValueAddress);
        const bool bChanged = ImGui::InputDouble(Label.c_str(), &Value);
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
        const FString NewValue(Buffer);

        CommitValue(bChanged, &NewValue);
        break;
    }

    case EPropertyType::Vector:
    {
        FVector Value = *static_cast<FVector*>(ValueAddress);
        const bool bChanged = ImGui::DragFloat3(Label.c_str(), &Value.x, 0.1f);
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
        ImGui::TextDisabled("%s (Unsupported Property Type)", Label.c_str());
        break;
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
    if (!IsOpen())
    {
        return;
    }

    bool bIsOpen = IsOpen();

    if (!ImGui::Begin("Details", &bIsOpen))
    {
        SetOpen(bIsOpen);
        ImGui::End();
        return;
    }

    if (Context == nullptr || Context->SelectedActor == nullptr)
    {
        ImGui::TextDisabled("No Actor selected.");
        ImGui::End();
        SetOpen(bIsOpen);
        return;
    }

    AActor* SelectedActor = Context->SelectedActor;

    ImGui::TextWrapped("Actor: %s", SelectedActor->GetName().c_str());
    ImGui::Text("Actor UUID: %u", SelectedActor->GetUUID());

    if (ImGui::BeginPopup("AddComponentMenu"))
    {
        auto AddSceneComponent = [&](UActorComponent* Component)
        {
            if (Component == nullptr)
            {
                return;
            }

            if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
            {
                USceneComponent* Root = SelectedActor->GetRootComponent();

                if (Root != nullptr && Root != SceneComponent && !SceneComponent->AttachToComponent(Root, false))
                {
                    SelectedActor->DestroyComponent(Component);
                    return;
                }
            }

            const uint32 PreviousSelectedUUID = Context->GetSelectedObject() != nullptr ? Context->GetSelectedObject()->GetUUID() : 0;

            Context->SelectComponent(Component);

            if (Context->UndoManager != nullptr)
            {
                Context->UndoManager->RecordComponentCreate(*Context, SelectedActor, Component, PreviousSelectedUUID);
            }
        };

        if (ImGui::MenuItem("Scene Component"))
        {
            AddSceneComponent(SelectedActor->CreateComponent<USceneComponent>(FName("SceneComponent")));
        }

        if (ImGui::MenuItem("Static Mesh Component"))
        {
            AddSceneComponent(SelectedActor->CreateComponent<UStaticMeshComponent>(FName("StaticMeshComponent")));
        }

        if (ImGui::MenuItem("Camera Component"))
        {
            AddSceneComponent(SelectedActor->CreateComponent<UCameraComponent>(FName("CameraComponent")));
        }

        ImGui::EndPopup();
    }

    if (ImGui::Button("Add Component"))
    {
        ImGui::OpenPopup("AddComponentMenu");
    }

    if (ImGui::GetContentRegionAvail().x > 150.0f) ImGui::SameLine();

    const bool bCanRemoveSelectedComponent = Context->SelectedComponent != nullptr && Context->SelectedComponent != SelectedActor->GetRootComponent();

    ImGui::BeginDisabled(!bCanRemoveSelectedComponent);

    if (ImGui::Button("Remove Component") && Context->SelectedComponent != nullptr)
    {
        UActorComponent* ComponentToRemove = Context->SelectedComponent;

        if (Context->UndoManager != nullptr)
        {
            Context->UndoManager->RecordComponentDelete(*Context, SelectedActor, ComponentToRemove);
        }

        Context->SelectActor(SelectedActor);
        SelectedActor->DestroyComponent(ComponentToRemove);
    }

    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::Text("Components");

    const float ComponentsHeight = (std::min)(150.0f, ImGui::GetContentRegionAvail().y * 0.28f);

    ImGui::BeginChild("##ComponentHierarchy", ImVec2(0.0f, ComponentsHeight), ImGuiChildFlags_Borders);

    USceneComponent* RootComponent = SelectedActor->GetRootComponent();

    if (RootComponent != nullptr)
    {
        DrawComponentHierarchy(RootComponent, *Context);
    }

    for (UActorComponent* Component : SelectedActor->GetComponents())
    {
        if (Component == nullptr)
        {
            continue;
        }

        USceneComponent* SceneComponent = Cast<USceneComponent>(Component);

        if (SceneComponent != nullptr)
        {
            if (SceneComponent != RootComponent && SceneComponent->GetAttachParent() == nullptr)
            {
                DrawComponentHierarchy(SceneComponent, *Context);
            }

            continue;
        }

        ImGui::PushID(Component);

        const bool bSelected = Context->SelectedComponent == Component;
        const FString ComponentName = Component->GetName();

        if (ImGui::Selectable(ComponentName.c_str(), bSelected, ImGuiSelectableFlags_SpanAvailWidth))
        {
            Context->SelectComponent(Component);
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    ImGui::Separator();

    ImGui::BeginChild("##ComponentProperties", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

    UObject* SelectedObject = Context->SelectedComponent != nullptr ? Context->SelectedComponent : SelectedActor->GetRootComponent();

    if (SelectedObject == nullptr)
    {
        ImGui::TextDisabled("No Component selected.");
        ImGui::EndChild();
        ImGui::End();
        SetOpen(bIsOpen);
        return;
    }

    UClass* ObjectClass = SelectedObject->GetClass();

    if (ObjectClass == nullptr)
    {
        ImGui::EndChild();
        ImGui::End();
        SetOpen(bIsOpen);
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
        if (Property == nullptr || !Property->IsVisible())
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
            if (Property != nullptr && Property->GetCategory() == Category)
            {
                DrawProperty(Context, SelectedObject, Property);
            }
        }
    }

    ImGui::EndChild();
    ImGui::End();
    SetOpen(bIsOpen);
}