#include "DetailsPanel.h"

#include "imgui.h"
#include "UObject/Class.h"

static void DrawProperty(UObject* Object, const FProperty* Property)
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
            Property->SetValue(Object, Property->GetType(), NewValue);
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

        if (ImGui::InputText(Label.c_str(), Buffer, sizeof(Buffer)))
        {
            FString NewValue(Buffer);
            CommitValue(true, &NewValue);
        }

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

        bool bChanged = ImGui::DragFloat3(Label.c_str(), Values, 0.1f);

        if (bChanged)
        {
            FRotator NewValue(Values[0], Values[1], Values[2]);
            CommitValue(true, &NewValue);
        }
        break;
    }

    case EPropertyType::Transform:
    {
        FTransform Value = *static_cast<FTransform*>(ValueAddress);

        if (ImGui::TreeNode(Label.c_str()))
        {
            bool bChanged = false;
            bChanged |= ImGui::DragFloat3("Location", &Value.Location.x, 0.1f);
            float Rotation[3] = {Value.Rotation.Pitch, Value.Rotation.Yaw, Value.Rotation.Roll};
            if (ImGui::DragFloat3("Rotation", Rotation, 0.1f))
            {
                Value.Rotation = FRotator(Rotation[0], Rotation[1], Rotation[2]);
                bChanged = true;
            }
            bChanged |= ImGui::DragFloat3("Scale", &Value.Scale3D.x, 0.1f);
            CommitValue(bChanged, &Value);
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
    if (Context == nullptr || Context->SelectedObject == nullptr)
    {
        ImGui::TextDisabled("No object selected.");
        ImGui::End();
        return;
    }
    UObject* SelectedObject = Context->SelectedObject;
    UClass* ObjectClass = SelectedObject->GetClass();
    if (ObjectClass == nullptr)
    {
        ImGui::End();
        return;
    }
    ImGui::Text("%s", ObjectClass->GetName().c_str());
    
    ImGui::Text("UUID: %u", SelectedObject->GetUUID());
    
    ImGui::Separator();
    
    TArray<const FProperty*> Properties;
    ObjectClass->GetProperties(Properties, true);
    
    for (const FProperty* Property : Properties)
    {
        DrawProperty(SelectedObject, Property);
    }
    ImGui::End();
}
