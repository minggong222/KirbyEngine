#pragma once

#include "EditorObjectContext.h"

#include "Components/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Core/EngineStatics.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "EditorTransformGizmo.h"
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
        Property,
        StaticMesh,
        Material
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
        UMaterial* Material = nullptr;

        std::vector<FPropertySnapshot> CameraProperties;
    };

    struct FHistoryEntry
    {
        EHistoryType Type = EHistoryType::Transform;
        FObjectSnapshot Object;

        FTransform BeforeTransform;
        FTransform AfterTransform;

        UStaticMesh* BeforeStaticMesh = nullptr;
        UStaticMesh* AfterStaticMesh = nullptr;

        UMaterial* BeforeMaterial = nullptr;
        UMaterial* AfterMaterial = nullptr;
        
        FString PropertyName;
        EPropertyType PropertyType = EPropertyType::Unknown;
        FPropertyValue BeforePropertyValue;
        FPropertyValue AfterPropertyValue;

        int32 ObjectIndex = INDEX_NONE;

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

        const uint32 PreviousSelectedUUID = Context.SelectedObject != nullptr ? Context.SelectedObject->GetUUID() : 0;

        FObjectSnapshot PasteSnapshot = *ClipboardSnapshot;

        ++ClipboardPasteCount;
        PasteSnapshot.Transform.Location.y += 1.5f * static_cast<float>(ClipboardPasteCount);

        UObject* NewObjectInstance = RestoreObject(PasteSnapshot, false);

        if (NewObjectInstance == nullptr)
        {
            --ClipboardPasteCount;
            return nullptr;
        }

        Context.Objects.Add(NewObjectInstance);
        Context.SelectedObject = NewObjectInstance;

        RecordCreate(Context, NewObjectInstance, PreviousSelectedUUID);

        return NewObjectInstance;
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

        if (Object == nullptr || !CaptureObject(Object, Entry.Object))
        {
            return;
        }
        Entry.ObjectIndex = Context.Objects.Find(Object);
        Entry.Type = EHistoryType::ObjectPresence;
        Entry.bBeforePresent = false;
        Entry.bAfterPresent = true;
        Entry.BeforeSelectedUUID = PreviousSelectedUUID;
        Entry.AfterSelectedUUID = Object->GetUUID();

        PushEntry(std::move(Entry));
    }

    void RecordDelete(const FEditorObjectContext& Context, AActor* Actor)
    {
        if (Object == nullptr)
        {
            return;
        }

        CommitPendingPropertyEdit(Context);

        FHistoryEntry Entry;

        if (!CaptureObject(Object, Entry.Object))
        {
            return;
        }

        Entry.ObjectIndex = Context.Objects.Find(Object);

        if (Entry.ObjectIndex == INDEX_NONE)
        {
            return;
        }
        CommitPendingPropertyEdit(Context);
        Entry.Type = EHistoryType::ObjectPresence;
        Entry.bBeforePresent = true;
        Entry.bAfterPresent = false;
        Entry.BeforeSelectedUUID = Object->GetUUID();
        Entry.AfterSelectedUUID = 0;

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
    
    void RecordStaticMeshChange(const FEditorObjectContext& Context, UStaticMeshComponent* Component, UStaticMesh* Before, UStaticMesh* After)
    {
        CommitPendingPropertyEdit(Context);

        if (Component == nullptr || Before == After || !Context.Objects.Contains(Component))
        {
            return;
        }

        FHistoryEntry Entry;
        Entry.Type = EHistoryType::StaticMesh;
        Entry.Object.UUID = Component->GetUUID();
        Entry.BeforeStaticMesh = Before;
        Entry.AfterStaticMesh = After;
        Entry.BeforeSelectedUUID = Component->GetUUID();
        Entry.AfterSelectedUUID = Component->GetUUID();

        PushEntry(std::move(Entry));
    }

    void RecordMaterialChange(const FEditorObjectContext& Context, UStaticMeshComponent* Component, UMaterial* Before, UMaterial* After)
    {
        CommitPendingPropertyEdit(Context);

        if (Component == nullptr || Before == After || !Context.Objects.Contains(Component))
        {
            return;
        }

        FHistoryEntry Entry;
        Entry.Type = EHistoryType::Material;
        Entry.Object.UUID = Component->GetUUID();
        Entry.BeforeMaterial = Before;
        Entry.AfterMaterial = After;
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

    static uint32 GetSelectedUUID(const FEditorObjectContext& Context)
    {
        return Context.SelectedObject != nullptr ? Context.SelectedObject->GetUUID() : 0;
    }

    static bool CaptureObject(UObject* Object, FObjectSnapshot& OutSnapshot)
    {
        OutSnapshot.UUID = Object->GetUUID();

        if (UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Object))
        {
            if (Component->GetStaticMesh() == nullptr)
            {
                return false;
            }

            OutSnapshot.Type = EObjectType::StaticMesh;
            OutSnapshot.StaticMesh = Component->GetStaticMesh();
            OutSnapshot.Material = Component->GetMaterial();
            OutSnapshot.Transform = Component->GetWorldTransform();
            return true;
        }

        if (UCameraComponent* Camera = Cast<UCameraComponent>(Object))
        {
            OutSnapshot.Type = EObjectType::Camera;
            OutSnapshot.Transform = Camera->GetWorldTransform();

            TArray<const FProperty*> Properties;
            Camera->GetClass()->GetProperties(Properties, true);

            for (const FProperty* Property : Properties)
            {
                if (Property->GetName().ToStd() == "Transform")
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

                switch (Property->GetType())
                {
                case EPropertyType::Bool:
                    PropertySnapshot.Value = *static_cast<const bool*>(ValuePtr);
                    break;

                case EPropertyType::Int32:
                    PropertySnapshot.Value = *static_cast<const int32*>(ValuePtr);
                    break;

                case EPropertyType::UInt32:
                    PropertySnapshot.Value = *static_cast<const uint32*>(ValuePtr);
                    break;

                case EPropertyType::Float:
                    PropertySnapshot.Value = *static_cast<const float*>(ValuePtr);
                    break;

                case EPropertyType::Double:
                    PropertySnapshot.Value = *static_cast<const double*>(ValuePtr);
                    break;

                case EPropertyType::String:
                    PropertySnapshot.Value = *static_cast<const FString*>(ValuePtr);
                    break;

                case EPropertyType::Vector:
                    PropertySnapshot.Value = *static_cast<const FVector*>(ValuePtr);
                    break;

                case EPropertyType::Rotator:
                    PropertySnapshot.Value = *static_cast<const FRotator*>(ValuePtr);
                    break;

                case EPropertyType::Transform:
                    PropertySnapshot.Value = *static_cast<const FTransform*>(ValuePtr);
                    break;

                default:
                    return false;
                }

                OutSnapshot.CameraProperties.push_back(std::move(PropertySnapshot));
            }

            return true;
        }

        return false;
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
                Component->SetStaticMesh(Snapshot.StaticMesh);
                Component->SetMaterial(Snapshot.Material);

                if (bPreserveUUID)
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

                if (bPreserveUUID)
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

            if (Snapshot.UUID < (std::numeric_limits<uint32>::max)() && FEngineStatics::NextUUID <= Snapshot.UUID)
            {
                FEngineStatics::NextUUID = Snapshot.UUID + 1;
            }
        }

        return Actor;
    }

    static bool RemoveObject(FEditorObjectContext& Context, uint32 UUID)
    {
        UObject* Object = FindObject(Context, UUID);

        if (Object == nullptr)
        {
            return true;
        }

        const int32 ObjectIndex = Context.Objects.Find(Object);

        if (ObjectIndex == INDEX_NONE)
        {
            return false;
        }

        if (Context.SelectedObject == Object)
        {
            Context.SelectedObject = nullptr;
        }

        Context.Objects.RemoveAt(ObjectIndex);
        delete Object;

        return true;
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
        }
        else if (Entry.Type == EHistoryType::Material)
        {
            UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(FindObject(Context, Entry.Object.UUID));

            if (Component == nullptr)
            {
                return false;
            }

            Component->SetMaterial(bUndo ? Entry.BeforeMaterial : Entry.AfterMaterial);
        }
        else
        {
            const bool bShouldExist = bUndo ? Entry.bBeforePresent : Entry.bAfterPresent;
            UObject* ExistingObject = FindObject(Context, Entry.Object.UUID);

            if (bShouldExist && ExistingObject == nullptr)
            {
                UObject* RestoredObject = RestoreObject(Entry.Object);

                if (RestoredObject == nullptr)
                {
                    return false;
                }

                int32 InsertIndex = Entry.ObjectIndex;

                if (InsertIndex == INDEX_NONE)
                {
                    InsertIndex = Context.Objects.Num();
                }

                InsertIndex = std::clamp(InsertIndex, 0, Context.Objects.Num());

                Context.Objects.Insert(RestoredObject, InsertIndex);
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
        Context.SelectedObject = FindObject(Context, SelectedUUID);

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
