#pragma once

#include "EditorObjectContext.h"

#include "Components/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Core/EngineStatics.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "Viewport/EditorTransformGizmo.h"
#include "Core/Math/MathUtility.h"
#include "Resources/Material.h"

#include "Actors/Actor.h"
#include "Actors/StaticMeshActor.h"
#include "Actors/CameraActor.h"

#include "World/World.h"
#include "World/Level.h"

#include <algorithm>
#include <cmath>
#include <variant>
#include <vector>
#include <limits>
#include <utility>
#include <optional>

class FEditorUndoManager
{
private:
    using FPropertyValue = std::variant<bool, int32, uint32, float, double, FString, FVector, FRotator, FTransform>;

    enum class EHistoryType
    {
        Transform,
        ObjectPresence,
        ComponentPresence,
        Property,
        StaticMesh,
        Material
    };

    enum class EComponentSnapshotType
    {
        Scene,
        StaticMesh,
        Camera
    };

    enum class EObjectType
    {
        Actor,
        StaticMeshActor,
        CameraActor
    };

    struct FPropertySnapshot
    {
        FString Name;
        EPropertyType Type = EPropertyType::Unknown;
        FPropertyValue Value;
    };

    struct FObjectSnapshot
    {
        uint32 UUID = 0;
        uint32 RootComponentUUID = 0;
        FName Name;
        EObjectType Type = EObjectType::Actor;
        FTransform Transform;

        UStaticMesh* StaticMesh = nullptr;
        TArray<UMaterial*> MaterialOverrides;
        TArray<uint8> MaterialOverrideMask;

        std::vector<FPropertySnapshot> CameraProperties;
    };

    struct FComponentSnapshot
    {
        uint32 OwnerUUID = 0;
        uint32 UUID = 0;
        uint32 ParentUUID = 0;
        FName Name;
        EComponentSnapshotType Type = EComponentSnapshotType::Scene;
        FTransform RelativeTransform;
        bool bWasRoot = false;

        UStaticMesh* StaticMesh = nullptr;
        TArray<UMaterial*> MaterialOverrides;
        TArray<uint8> MaterialOverrideMask;

        std::vector<FPropertySnapshot> Properties;
        std::vector<uint32> ChildComponentUUIDs;
    };

    struct FHistoryEntry
    {
        EHistoryType Type = EHistoryType::Transform;

        FObjectSnapshot Object;
        FComponentSnapshot Component;

        FTransform BeforeTransform;
        FTransform AfterTransform;

        UStaticMesh* BeforeStaticMesh = nullptr;
        UStaticMesh* AfterStaticMesh = nullptr;

        TArray<UMaterial*> BeforeMaterialOverrides;
        TArray<uint8> BeforeMaterialOverrideMask;
        TArray<UMaterial*> AfterMaterialOverrides;
        TArray<uint8> AfterMaterialOverrideMask;

        int32 MaterialIndex = 0;
        UMaterial* BeforeMaterial = nullptr;
        UMaterial* AfterMaterial = nullptr;
        bool bBeforeMaterialOverride = false;
        bool bAfterMaterialOverride = false;

        FString PropertyName;
        EPropertyType PropertyType = EPropertyType::Unknown;
        FPropertyValue BeforePropertyValue;
        FPropertyValue AfterPropertyValue;

        bool bBeforePresent = true;
        bool bAfterPresent = true;

        uint32 BeforeSelectedUUID = 0;
        uint32 AfterSelectedUUID = 0;
    };

    struct FPendingPropertyEdit
    {
        uint32 ObjectUUID = 0;
        FString PropertyName;
        EPropertyType PropertyType = EPropertyType::Unknown;
        FPropertyValue BeforeValue;
    };

public:
    void Clear()
    {
        UndoStack.clear();
        RedoStack.clear();
        ClipboardSnapshot.reset();
        ClipboardPasteCount = 0;
        PendingPropertyEdit.reset();
    }

    bool CanUndo() const
    {
        return !UndoStack.empty();
    }

    bool CanRedo() const
    {
        return !RedoStack.empty();
    }

    bool CanPaste() const
    {
        return ClipboardSnapshot.has_value();
    }

    bool CopySelected(const FEditorObjectContext& Context)
    {
        if (Context.SelectedActor == nullptr)
        {
            return false;
        }

        FObjectSnapshot Snapshot;

        if (!CaptureObject(Context.SelectedActor, Snapshot))
        {
            return false;
        }

        ClipboardSnapshot = std::move(Snapshot);
        ClipboardPasteCount = 0;

        return true;
    }

    UObject* PasteCopied(FEditorObjectContext& Context)
    {
        if (!ClipboardSnapshot.has_value())
        {
            return nullptr;
        }

        if (Context.TransformGizmo != nullptr)
        {
            Context.TransformGizmo->CancelDrag();
        }

        const uint32 PreviousSelectedUUID = GetSelectedUUID(Context);

        FObjectSnapshot PasteSnapshot = *ClipboardSnapshot;

        ++ClipboardPasteCount;
        PasteSnapshot.Transform.Location.y += 1.5f * static_cast<float>(ClipboardPasteCount);

        AActor* NewActor = RestoreObject(Context, PasteSnapshot, false);

        if (NewActor == nullptr)
        {
            --ClipboardPasteCount;
            return nullptr;
        }

        Context.SelectActor(NewActor);
        RecordCreate(Context, NewActor, PreviousSelectedUUID);

        return NewActor;
    }

    bool BeginPropertyEdit(const FEditorObjectContext& Context, UObject* Object, const FProperty* Property)
    {
        if (Object == nullptr || Property == nullptr || !Property->IsEditable())
        {
            return false;
        }

        const uint32 ObjectUUID = Object->GetUUID();

        if (PendingPropertyEdit.has_value() && PendingPropertyEdit->ObjectUUID == ObjectUUID && PendingPropertyEdit->PropertyName == Property->GetName())
        {
            return true;
        }

        CommitPendingPropertyEdit(Context);

        const void* ValuePtr = Property->GetValuePtr(Object);
        FPropertyValue BeforeValue;

        if (!ReadPropertyValue(Property->GetType(), ValuePtr, BeforeValue))
        {
            return false;
        }

        FPendingPropertyEdit Pending;
        Pending.ObjectUUID = ObjectUUID;
        Pending.PropertyName = Property->GetName();
        Pending.PropertyType = Property->GetType();
        Pending.BeforeValue = std::move(BeforeValue);

        PendingPropertyEdit = std::move(Pending);

        return true;
    }

    bool EndPropertyEdit(const FEditorObjectContext& Context, UObject* Object, const FProperty* Property)
    {
        if (!PendingPropertyEdit.has_value() || Object == nullptr || Property == nullptr)
        {
            return false;
        }

        if (PendingPropertyEdit->ObjectUUID != Object->GetUUID() || PendingPropertyEdit->PropertyName != Property->GetName())
        {
            return false;
        }

        return CommitPendingPropertyEdit(Context);
    }

    void RecordCreate(const FEditorObjectContext& Context, AActor* Actor, uint32 PreviousSelectedUUID)
    {
        CommitPendingPropertyEdit(Context);

        FHistoryEntry Entry;

        if (Actor == nullptr || !CaptureObject(Actor, Entry.Object))
        {
            return;
        }

        Entry.Type = EHistoryType::ObjectPresence;
        Entry.bBeforePresent = false;
        Entry.bAfterPresent = true;
        Entry.BeforeSelectedUUID = PreviousSelectedUUID;
        Entry.AfterSelectedUUID = Actor->GetUUID();

        PushEntry(std::move(Entry));
    }

    void RecordDelete(const FEditorObjectContext& Context, AActor* Actor)
    {
        if (Actor == nullptr)
        {
            return;
        }

        CommitPendingPropertyEdit(Context);

        FHistoryEntry Entry;

        if (!CaptureObject(Actor, Entry.Object))
        {
            return;
        }

        Entry.Type = EHistoryType::ObjectPresence;
        Entry.bBeforePresent = true;
        Entry.bAfterPresent = false;
        Entry.BeforeSelectedUUID = GetSelectedUUID(Context);
        Entry.AfterSelectedUUID = 0;

        PushEntry(std::move(Entry));
    }

    void RecordComponentCreate(const FEditorObjectContext& Context, AActor* Owner, UActorComponent* Component, uint32 PreviousSelectedUUID)
    {
        CommitPendingPropertyEdit(Context);

        FHistoryEntry Entry;

        if (Owner == nullptr || Component == nullptr || Component->GetOwner() != Owner || !CaptureComponent(Context, Component, Entry.Component))
        {
            return;
        }

        Entry.Type = EHistoryType::ComponentPresence;
        Entry.Object.UUID = Component->GetUUID();
        Entry.bBeforePresent = false;
        Entry.bAfterPresent = true;
        Entry.BeforeSelectedUUID = PreviousSelectedUUID;
        Entry.AfterSelectedUUID = Component->GetUUID();

        PushEntry(std::move(Entry));
    }

    void RecordComponentDelete(const FEditorObjectContext& Context, AActor* Owner, UActorComponent* Component)
    {
        CommitPendingPropertyEdit(Context);

        FHistoryEntry Entry;

        if (Owner == nullptr || Component == nullptr || Component->GetOwner() != Owner || !CaptureComponent(Context, Component, Entry.Component))
        {
            return;
        }

        Entry.Type = EHistoryType::ComponentPresence;
        Entry.Object.UUID = Component->GetUUID();
        Entry.bBeforePresent = true;
        Entry.bAfterPresent = false;
        Entry.BeforeSelectedUUID = Component->GetUUID();
        Entry.AfterSelectedUUID = Owner->GetUUID();

        PushEntry(std::move(Entry));
    }

    void RecordTransform(const FEditorObjectContext& Context, uint32 UUID, const FTransform& Before, const FTransform& After)
    {
        CommitPendingPropertyEdit(Context);

        if (!IsTransformDifferent(Before, After))
        {
            return;
        }

        FHistoryEntry Entry;
        Entry.Type = EHistoryType::Transform;
        Entry.Object.UUID = UUID;
        Entry.BeforeTransform = Before;
        Entry.AfterTransform = After;
        Entry.BeforeSelectedUUID = UUID;
        Entry.AfterSelectedUUID = UUID;

        PushEntry(std::move(Entry));
    }

    void RecordStaticMeshChange(const FEditorObjectContext& Context, UStaticMeshComponent* Component, UStaticMesh* Before, UStaticMesh* After, const TArray<UMaterial*>& BeforeOverrides, const TArray<uint8>& BeforeOverrideMask, const TArray<UMaterial*>& AfterOverrides, const TArray<uint8>& AfterOverrideMask)
    {
        CommitPendingPropertyEdit(Context);

        if (Component == nullptr || Before == After || !IsComponentInWorld(Context, Component))
        {
            return;
        }

        FHistoryEntry Entry;
        Entry.Type = EHistoryType::StaticMesh;
        Entry.Object.UUID = Component->GetUUID();

        Entry.BeforeStaticMesh = Before;
        Entry.AfterStaticMesh = After;

        Entry.BeforeMaterialOverrides = BeforeOverrides;
        Entry.BeforeMaterialOverrideMask = BeforeOverrideMask;
        Entry.AfterMaterialOverrides = AfterOverrides;
        Entry.AfterMaterialOverrideMask = AfterOverrideMask;

        Entry.BeforeSelectedUUID = Component->GetUUID();
        Entry.AfterSelectedUUID = Component->GetUUID();

        PushEntry(std::move(Entry));
    }

    void RecordMaterialChange(const FEditorObjectContext& Context, UStaticMeshComponent* Component, int32 MaterialIndex, UMaterial* Before, bool bBeforeOverride, UMaterial* After, bool bAfterOverride)
    {
        CommitPendingPropertyEdit(Context);

        if (Component == nullptr || MaterialIndex < 0 || MaterialIndex >= 4096 || !IsComponentInWorld(Context, Component))
        {
            return;
        }

        if (Before == After && bBeforeOverride == bAfterOverride)
        {
            return;
        }

        FHistoryEntry Entry;
        Entry.Type = EHistoryType::Material;
        Entry.Object.UUID = Component->GetUUID();

        Entry.MaterialIndex = MaterialIndex;
        Entry.BeforeMaterial = Before;
        Entry.AfterMaterial = After;
        Entry.bBeforeMaterialOverride = bBeforeOverride;
        Entry.bAfterMaterialOverride = bAfterOverride;

        Entry.BeforeSelectedUUID = Component->GetUUID();
        Entry.AfterSelectedUUID = Component->GetUUID();

        PushEntry(std::move(Entry));
    }

    void Undo(FEditorObjectContext& Context)
    {
        CommitPendingPropertyEdit(Context);

        if (Context.TransformGizmo != nullptr)
        {
            Context.TransformGizmo->CancelDrag();
        }

        if (UndoStack.empty())
        {
            return;
        }

        FHistoryEntry Entry = std::move(UndoStack.back());
        UndoStack.pop_back();

        if (!ApplyEntry(Context, Entry, true))
        {
            UndoStack.push_back(std::move(Entry));
            return;
        }

        RedoStack.push_back(std::move(Entry));
    }

    void Redo(FEditorObjectContext& Context)
    {
        CommitPendingPropertyEdit(Context);

        if (Context.TransformGizmo != nullptr)
        {
            Context.TransformGizmo->CancelDrag();
        }

        if (RedoStack.empty())
        {
            return;
        }

        FHistoryEntry Entry = std::move(RedoStack.back());
        RedoStack.pop_back();

        if (!ApplyEntry(Context, Entry, false))
        {
            RedoStack.push_back(std::move(Entry));
            return;
        }

        UndoStack.push_back(std::move(Entry));
    }

private:
    static bool IsTransformDifferent(const FTransform& A, const FTransform& B)
    {
        constexpr float Tolerance = 0.0001f;

        return
            std::fabs(A.Location.x - B.Location.x) > Tolerance ||
            std::fabs(A.Location.y - B.Location.y) > Tolerance ||
            std::fabs(A.Location.z - B.Location.z) > Tolerance ||
            std::fabs(A.Rotation.Pitch - B.Rotation.Pitch) > Tolerance ||
            std::fabs(A.Rotation.Yaw - B.Rotation.Yaw) > Tolerance ||
            std::fabs(A.Rotation.Roll - B.Rotation.Roll) > Tolerance ||
            std::fabs(A.Scale3D.x - B.Scale3D.x) > Tolerance ||
            std::fabs(A.Scale3D.y - B.Scale3D.y) > Tolerance ||
            std::fabs(A.Scale3D.z - B.Scale3D.z) > Tolerance;
    }

    static bool ReadPropertyValue(EPropertyType Type, const void* ValuePtr, FPropertyValue& OutValue)
    {
        if (ValuePtr == nullptr)
        {
            return false;
        }

        switch (Type)
        {
        case EPropertyType::Bool:
            OutValue = *static_cast<const bool*>(ValuePtr);
            return true;

        case EPropertyType::Int32:
            OutValue = *static_cast<const int32*>(ValuePtr);
            return true;

        case EPropertyType::UInt32:
            OutValue = *static_cast<const uint32*>(ValuePtr);
            return true;

        case EPropertyType::Float:
            OutValue = *static_cast<const float*>(ValuePtr);
            return true;

        case EPropertyType::Double:
            OutValue = *static_cast<const double*>(ValuePtr);
            return true;

        case EPropertyType::String:
            OutValue = *static_cast<const FString*>(ValuePtr);
            return true;

        case EPropertyType::Vector:
            OutValue = *static_cast<const FVector*>(ValuePtr);
            return true;

        case EPropertyType::Rotator:
            OutValue = *static_cast<const FRotator*>(ValuePtr);
            return true;

        case EPropertyType::Transform:
            OutValue = *static_cast<const FTransform*>(ValuePtr);
            return true;

        default:
            return false;
        }
    }

    static bool ArePropertyValuesEqual(EPropertyType Type, const FPropertyValue& A, const FPropertyValue& B)
    {
        constexpr float Tolerance = 0.0001f;

        switch (Type)
        {
        case EPropertyType::Bool:
            return std::get<bool>(A) == std::get<bool>(B);

        case EPropertyType::Int32:
            return std::get<int32>(A) == std::get<int32>(B);

        case EPropertyType::UInt32:
            return std::get<uint32>(A) == std::get<uint32>(B);

        case EPropertyType::Float:
            return std::fabs(std::get<float>(A) - std::get<float>(B)) <= Tolerance;

        case EPropertyType::Double:
            return std::fabs(std::get<double>(A) - std::get<double>(B)) <= Tolerance;

        case EPropertyType::String:
            return std::get<FString>(A) == std::get<FString>(B);

        case EPropertyType::Vector:
        {
            const FVector& Left = std::get<FVector>(A);
            const FVector& Right = std::get<FVector>(B);

            return std::fabs(Left.x - Right.x) <= Tolerance && std::fabs(Left.y - Right.y) <= Tolerance && std::fabs(Left.z - Right.z) <= Tolerance;
        }

        case EPropertyType::Rotator:
        {
            const FRotator& Left = std::get<FRotator>(A);
            const FRotator& Right = std::get<FRotator>(B);

            return std::fabs(Left.Pitch - Right.Pitch) <= Tolerance && std::fabs(Left.Yaw - Right.Yaw) <= Tolerance && std::fabs(Left.Roll - Right.Roll) <= Tolerance;
        }

        case EPropertyType::Transform:
            return !IsTransformDifferent(std::get<FTransform>(A), std::get<FTransform>(B));

        default:
            return false;
        }
    }

    bool CommitPendingPropertyEdit(const FEditorObjectContext& Context)
    {
        if (!PendingPropertyEdit.has_value())
        {
            return false;
        }

        FPendingPropertyEdit Pending = std::move(*PendingPropertyEdit);
        PendingPropertyEdit.reset();

        UObject* Object = FindObject(Context, Pending.ObjectUUID);

        if (Object == nullptr || Object->GetClass() == nullptr)
        {
            return false;
        }

        const FProperty* Property = Object->GetClass()->FindProperty(Pending.PropertyName.c_str());

        if (Property == nullptr)
        {
            return false;
        }

        const void* ValuePtr = Property->GetValuePtr(Object);
        FPropertyValue AfterValue;

        if (!ReadPropertyValue(Property->GetType(), ValuePtr, AfterValue))
        {
            return false;
        }

        if (ArePropertyValuesEqual(Pending.PropertyType, Pending.BeforeValue, AfterValue))
        {
            return true;
        }

        FHistoryEntry Entry;
        Entry.Type = EHistoryType::Property;
        Entry.Object.UUID = Pending.ObjectUUID;
        Entry.PropertyName = Pending.PropertyName;
        Entry.PropertyType = Pending.PropertyType;
        Entry.BeforePropertyValue = std::move(Pending.BeforeValue);
        Entry.AfterPropertyValue = std::move(AfterValue);
        Entry.BeforeSelectedUUID = Pending.ObjectUUID;
        Entry.AfterSelectedUUID = Pending.ObjectUUID;

        PushEntry(std::move(Entry));

        return true;
    }

    static UObject* FindObject(const FEditorObjectContext& Context, uint32 UUID)
    {
        if (Context.World == nullptr || UUID == 0)
        {
            return nullptr;
        }

        ULevel* Level = Context.World->GetPersistentLevel();

        if (Level == nullptr)
        {
            return nullptr;
        }

        for (AActor* Actor : Level->GetActors())
        {
            if (Actor == nullptr)
            {
                continue;
            }

            if (Actor->GetUUID() == UUID)
            {
                return Actor;
            }

            for (UActorComponent* Component : Actor->GetComponents())
            {
                if (Component != nullptr && Component->GetUUID() == UUID)
                {
                    return Component;
                }
            }
        }

        return nullptr;
    }

    static AActor* FindActor(const FEditorObjectContext& Context, uint32 UUID)
    {
        return Cast<AActor>(FindObject(Context, UUID));
    }

    static bool IsComponentInWorld(const FEditorObjectContext& Context, const UActorComponent* Component)
    {
        if (Context.World == nullptr || Component == nullptr || Component->GetOwner() == nullptr)
        {
            return false;
        }

        ULevel* Level = Context.World->GetPersistentLevel();

        return Level != nullptr && Level->GetActors().Contains(Component->GetOwner());
    }

    static uint32 GetSelectedUUID(const FEditorObjectContext& Context)
    {
        UObject* SelectedObject = Context.GetSelectedObject();
        return SelectedObject != nullptr ? SelectedObject->GetUUID() : 0;
    }

    static bool CaptureObject(AActor* Actor, FObjectSnapshot& OutSnapshot)
    {
        if (Actor == nullptr)
        {
            return false;
        }

        OutSnapshot.UUID = Actor->GetUUID();
        OutSnapshot.Name = Actor->GetFName();
        OutSnapshot.Transform = Actor->GetActorTransform();

        USceneComponent* RootComponent = Actor->GetRootComponent();
        OutSnapshot.RootComponentUUID = RootComponent != nullptr ? RootComponent->GetUUID() : 0;

        if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
        {
            UStaticMeshComponent* Component = StaticMeshActor->GetStaticMeshComponent();

            if (Component == nullptr || Component->GetStaticMesh() == nullptr)
            {
                return false;
            }

            OutSnapshot.Type = EObjectType::StaticMeshActor;
            OutSnapshot.StaticMesh = Component->GetStaticMesh();
            OutSnapshot.MaterialOverrides = Component->GetMaterialOverrides();
            OutSnapshot.MaterialOverrideMask = Component->GetMaterialOverrideMask();

            return true;
        }

        if (ACameraActor* CameraActor = Cast<ACameraActor>(Actor))
        {
            UCameraComponent* Camera = CameraActor->GetCameraComponent();

            if (Camera == nullptr)
            {
                return false;
            }

            OutSnapshot.Type = EObjectType::CameraActor;

            TArray<const FProperty*> Properties;
            Camera->GetClass()->GetProperties(Properties, true);

            for (const FProperty* Property : Properties)
            {
                if (Property == nullptr || Property->GetName().ToStd() == "Transform")
                {
                    continue;
                }

                const void* ValuePtr = Property->GetValuePtr(Camera);

                if (ValuePtr == nullptr)
                {
                    return false;
                }

                FPropertySnapshot PropertySnapshot;
                PropertySnapshot.Name = Property->GetName();
                PropertySnapshot.Type = Property->GetType();

                if (!ReadPropertyValue(Property->GetType(), ValuePtr, PropertySnapshot.Value))
                {
                    return false;
                }

                OutSnapshot.CameraProperties.push_back(std::move(PropertySnapshot));
            }

            return true;
        }

        if (!Actor->GetComponents().IsEmpty() || RootComponent != nullptr)
        {
            return false;
        }

        OutSnapshot.Type = EObjectType::Actor;
        return true;
    }

    static bool CaptureComponent(const FEditorObjectContext& Context, UActorComponent* Component, FComponentSnapshot& OutSnapshot)
    {
        if (Component == nullptr || !IsComponentInWorld(Context, Component))
        {
            return false;
        }

        AActor* Owner = Component->GetOwner();

        if (Owner == nullptr)
        {
            return false;
        }

        OutSnapshot.OwnerUUID = Owner->GetUUID();
        OutSnapshot.UUID = Component->GetUUID();
        OutSnapshot.Name = Component->GetFName();

        if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
        {
            OutSnapshot.RelativeTransform = SceneComponent->GetRelativeTransform();
            OutSnapshot.ParentUUID = SceneComponent->GetAttachParent() != nullptr ? SceneComponent->GetAttachParent()->GetUUID() : 0;
            OutSnapshot.bWasRoot = Owner->GetRootComponent() == SceneComponent;

            for (USceneComponent* Child : SceneComponent->GetAttachChildren())
            {
                if (Child != nullptr)
                {
                    OutSnapshot.ChildComponentUUIDs.push_back(Child->GetUUID());
                }
            }
        }

        if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
        {
            OutSnapshot.Type = EComponentSnapshotType::Camera;

            TArray<const FProperty*> Properties;
            Camera->GetClass()->GetProperties(Properties, true);

            for (const FProperty* Property : Properties)
            {
                if (Property == nullptr || Property->GetName().ToStd() == "Transform")
                {
                    continue;
                }

                FPropertySnapshot PropertySnapshot;
                PropertySnapshot.Name = Property->GetName();
                PropertySnapshot.Type = Property->GetType();

                if (!ReadPropertyValue(Property->GetType(), Property->GetValuePtr(Camera), PropertySnapshot.Value))
                {
                    return false;
                }

                OutSnapshot.Properties.push_back(std::move(PropertySnapshot));
            }

            return true;
        }

        if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
        {
            OutSnapshot.Type = EComponentSnapshotType::StaticMesh;
            OutSnapshot.StaticMesh = MeshComponent->GetStaticMesh();
            OutSnapshot.MaterialOverrides = MeshComponent->GetMaterialOverrides();
            OutSnapshot.MaterialOverrideMask = MeshComponent->GetMaterialOverrideMask();

            return true;
        }

        if (Cast<USceneComponent>(Component) != nullptr)
        {
            OutSnapshot.Type = EComponentSnapshotType::Scene;
            return true;
        }

        return false;
    }

    static UActorComponent* RestoreComponent(FEditorObjectContext& Context, const FComponentSnapshot& Snapshot)
    {
        AActor* Owner = FindActor(Context, Snapshot.OwnerUUID);

        if (Owner == nullptr || FindObject(Context, Snapshot.UUID) != nullptr)
        {
            return nullptr;
        }

        UActorComponent* Component = nullptr;

        switch (Snapshot.Type)
        {
        case EComponentSnapshotType::Scene:
            Component = Owner->CreateComponent<USceneComponent>(Snapshot.Name);
            break;

        case EComponentSnapshotType::StaticMesh:
            Component = Owner->CreateComponent<UStaticMeshComponent>(Snapshot.Name);
            break;

        case EComponentSnapshotType::Camera:
            Component = Owner->CreateComponent<UCameraComponent>(Snapshot.Name);
            break;

        default:
            return nullptr;
        }

        if (Component == nullptr)
        {
            return nullptr;
        }

        Component->SetUUID(Snapshot.UUID);

        if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component))
        {
            MeshComponent->SetStaticMesh(Snapshot.StaticMesh);
            MeshComponent->SetMaterialOverrides(Snapshot.MaterialOverrides, Snapshot.MaterialOverrideMask);
        }

        if (UCameraComponent* Camera = Cast<UCameraComponent>(Component))
        {
            for (const FPropertySnapshot& PropertySnapshot : Snapshot.Properties)
            {
                const FProperty* Property = Camera->GetClass()->FindProperty(PropertySnapshot.Name.c_str());

                if (Property == nullptr || !std::visit([&](const auto& Value) { return Property->SetValue(Camera, PropertySnapshot.Type, &Value); }, PropertySnapshot.Value))
                {
                    Owner->DestroyComponent(Component);
                    return nullptr;
                }
            }
        }

        if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
        {
            SceneComponent->SetRelativeTransform(Snapshot.RelativeTransform);

            if (Snapshot.bWasRoot)
            {
                if (!Owner->SetRootComponent(SceneComponent))
                {
                    Owner->DestroyComponent(Component);
                    return nullptr;
                }
            }
            else if (Snapshot.ParentUUID != 0)
            {
                USceneComponent* Parent = Cast<USceneComponent>(FindObject(Context, Snapshot.ParentUUID));

                if (Parent == nullptr || !SceneComponent->AttachToComponent(Parent, false))
                {
                    Owner->DestroyComponent(Component);
                    return nullptr;
                }
            }
        }

        if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
        {
            for (uint32 ChildUUID : Snapshot.ChildComponentUUIDs)
            {
                USceneComponent* Child = Cast<USceneComponent>(FindObject(Context, ChildUUID));

                if (Child != nullptr && !Child->AttachToComponent(SceneComponent, true))
                {
                    Owner->DestroyComponent(Component);
                    return nullptr;
                }
            }
        }

        if (Snapshot.UUID < (std::numeric_limits<uint32>::max)() && FEngineStatics::NextUUID <= Snapshot.UUID)
        {
            FEngineStatics::NextUUID = Snapshot.UUID + 1;
        }

        return Component;
    }

    static bool RemoveComponent(FEditorObjectContext& Context, uint32 ComponentUUID)
    {
        UActorComponent* Component = Cast<UActorComponent>(FindObject(Context, ComponentUUID));

        if (Component == nullptr)
        {
            return true;
        }

        AActor* Owner = Component->GetOwner();

        if (Owner == nullptr)
        {
            return false;
        }

        if (Context.SelectedComponent == Component)
        {
            Context.SelectActor(Owner);
        }

        return Owner->DestroyComponent(Component);
    }

    static AActor* RestoreObject(FEditorObjectContext& Context, const FObjectSnapshot& Snapshot, bool bPreserveUUID = true)
    {
        if (Context.World == nullptr)
        {
            return nullptr;
        }

        AActor* Actor = nullptr;

        if (Snapshot.Type == EObjectType::StaticMeshActor)
        {
            AStaticMeshActor* StaticMeshActor = Context.World->SpawnActor<AStaticMeshActor>(Snapshot.Name, Snapshot.Transform);

            if (StaticMeshActor != nullptr)
            {
                UStaticMeshComponent* Component = StaticMeshActor->GetStaticMeshComponent();

                if (Component == nullptr)
                {
                    Context.World->DestroyActor(StaticMeshActor);
                    return nullptr;
                }

                Component->SetStaticMesh(Snapshot.StaticMesh);
                Component->SetMaterialOverrides(Snapshot.MaterialOverrides, Snapshot.MaterialOverrideMask);

                if (bPreserveUUID && Snapshot.RootComponentUUID != 0)
                {
                    Component->SetUUID(Snapshot.RootComponentUUID);
                }

                Actor = StaticMeshActor;
            }
        }
        else if (Snapshot.Type == EObjectType::CameraActor)
        {
            ACameraActor* CameraActor = Context.World->SpawnActor<ACameraActor>(Snapshot.Name, Snapshot.Transform);

            if (CameraActor != nullptr)
            {
                UCameraComponent* Component = CameraActor->GetCameraComponent();

                if (Component == nullptr)
                {
                    Context.World->DestroyActor(CameraActor);
                    return nullptr;
                }

                for (const FPropertySnapshot& PropertySnapshot : Snapshot.CameraProperties)
                {
                    const FProperty* Property = Component->GetClass()->FindProperty(PropertySnapshot.Name.c_str());

                    if (Property == nullptr)
                    {
                        Context.World->DestroyActor(CameraActor);
                        return nullptr;
                    }

                    const bool bApplied = std::visit([&](const auto& Value)
                    {
                        return Property->SetValue(Component, PropertySnapshot.Type, &Value);
                    }, PropertySnapshot.Value);

                    if (!bApplied)
                    {
                        Context.World->DestroyActor(CameraActor);
                        return nullptr;
                    }
                }

                if (bPreserveUUID && Snapshot.RootComponentUUID != 0)
                {
                    Component->SetUUID(Snapshot.RootComponentUUID);
                }

                Actor = CameraActor;
            }
        }
        else
        {
            Actor = Context.World->SpawnActor<AActor>(Snapshot.Name, Snapshot.Transform);
        }

        if (Actor != nullptr && bPreserveUUID)
        {
            Actor->SetUUID(Snapshot.UUID);

            USceneComponent* RootComponent = Actor->GetRootComponent();

            if (RootComponent != nullptr && Snapshot.RootComponentUUID != 0)
            {
                RootComponent->SetUUID(Snapshot.RootComponentUUID);
            }

            const uint32 MaxUUID = (std::max)(Snapshot.UUID, Snapshot.RootComponentUUID);

            if (MaxUUID < (std::numeric_limits<uint32>::max)() && FEngineStatics::NextUUID <= MaxUUID)
            {
                FEngineStatics::NextUUID = MaxUUID + 1;
            }
        }

        return Actor;
    }

    static bool RemoveObject(FEditorObjectContext& Context, uint32 UUID)
    {
        AActor* Actor = FindActor(Context, UUID);

        if (Actor == nullptr)
        {
            return true;
        }

        if (Context.SelectedActor == Actor)
        {
            Context.ClearSelection();
        }

        return Context.World != nullptr && Context.World->DestroyActor(Actor);
    }

    static bool ApplyEntry(FEditorObjectContext& Context, const FHistoryEntry& Entry, bool bUndo)
    {
        if (Entry.Type == EHistoryType::Transform)
        {
            UObject* Object = FindObject(Context, Entry.Object.UUID);
            USceneComponent* Component = Cast<USceneComponent>(Object);

            if (Component == nullptr)
            {
                return false;
            }

            if (!Component->SetWorldTransform(bUndo ? Entry.BeforeTransform : Entry.AfterTransform))
            {
                return false;
            }
        }
        else if (Entry.Type == EHistoryType::Property)
        {
            UObject* Object = FindObject(Context, Entry.Object.UUID);

            if (Object == nullptr || Object->GetClass() == nullptr)
            {
                return false;
            }

            const FProperty* Property = Object->GetClass()->FindProperty(Entry.PropertyName.c_str());

            if (Property == nullptr)
            {
                return false;
            }

            const FPropertyValue& Value = bUndo ? Entry.BeforePropertyValue : Entry.AfterPropertyValue;

            const bool bApplied = std::visit([&](const auto& TypedValue)
            {
                return Property->SetValue(Object, Entry.PropertyType, &TypedValue);
            }, Value);

            if (!bApplied)
            {
                return false;
            }
        }
        else if (Entry.Type == EHistoryType::StaticMesh)
        {
            UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(FindObject(Context, Entry.Object.UUID));

            if (Component == nullptr)
            {
                return false;
            }

            Component->SetStaticMesh(bUndo ? Entry.BeforeStaticMesh : Entry.AfterStaticMesh);

            Component->SetMaterialOverrides(
                bUndo ? Entry.BeforeMaterialOverrides : Entry.AfterMaterialOverrides,
                bUndo ? Entry.BeforeMaterialOverrideMask : Entry.AfterMaterialOverrideMask
            );
        }
        else if (Entry.Type == EHistoryType::Material)
        {
            UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(FindObject(Context, Entry.Object.UUID));

            if (Component == nullptr)
            {
                return false;
            }

            const bool bShouldOverride = bUndo ? Entry.bBeforeMaterialOverride : Entry.bAfterMaterialOverride;
            UMaterial* Material = bUndo ? Entry.BeforeMaterial : Entry.AfterMaterial;

            if (bShouldOverride)
            {
                Component->SetMaterial(Entry.MaterialIndex, Material);
            }
            else
            {
                Component->ClearMaterialOverride(Entry.MaterialIndex);
            }
        }
        else if (Entry.Type == EHistoryType::ComponentPresence)
        {
            const bool bShouldExist = bUndo ? Entry.bBeforePresent : Entry.bAfterPresent;
            UActorComponent* ExistingComponent = Cast<UActorComponent>(FindObject(Context, Entry.Object.UUID));

            if (bShouldExist && ExistingComponent == nullptr)
            {
                if (RestoreComponent(Context, Entry.Component) == nullptr)
                {
                    return false;
                }
            }
            else if (!bShouldExist && ExistingComponent != nullptr)
            {
                if (!RemoveComponent(Context, Entry.Object.UUID))
                {
                    return false;
                }
            }
        }
        else
        {
            const bool bShouldExist = bUndo ? Entry.bBeforePresent : Entry.bAfterPresent;
            UObject* ExistingObject = FindObject(Context, Entry.Object.UUID);

            if (bShouldExist && ExistingObject == nullptr)
            {
                AActor* RestoredActor = RestoreObject(Context, Entry.Object);

                if (RestoredActor == nullptr)
                {
                    return false;
                }
            }
            else if (!bShouldExist && ExistingObject != nullptr)
            {
                if (!RemoveObject(Context, Entry.Object.UUID))
                {
                    return false;
                }
            }
        }

        const uint32 SelectedUUID = bUndo ? Entry.BeforeSelectedUUID : Entry.AfterSelectedUUID;
        UObject* SelectedObject = FindObject(Context, SelectedUUID);

        if (AActor* Actor = Cast<AActor>(SelectedObject))
        {
            Context.SelectActor(Actor);
        }
        else if (UActorComponent* Component = Cast<UActorComponent>(SelectedObject))
        {
            Context.SelectComponent(Component);
        }
        else
        {
            Context.ClearSelection();
        }

        return true;
    }

    void PushEntry(FHistoryEntry&& Entry)
    {
        RedoStack.clear();
        UndoStack.push_back(std::move(Entry));

        constexpr std::size_t MaxHistoryCount = 128;

        if (UndoStack.size() > MaxHistoryCount)
        {
            UndoStack.erase(UndoStack.begin());
        }
    }

    std::vector<FHistoryEntry> UndoStack;
    std::vector<FHistoryEntry> RedoStack;

    std::optional<FObjectSnapshot> ClipboardSnapshot;
    uint32 ClipboardPasteCount = 0;

    std::optional<FPendingPropertyEdit> PendingPropertyEdit;
};