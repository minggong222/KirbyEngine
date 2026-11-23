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


#include <algorithm>
#include <cmath>
#include <variant>
#include <vector>
#include <limits>
#include <utility>

class FEditorUndoManager
{
private:
    using FPropertyValue = std::variant<bool, int32, uint32, float, double, FString, FVector, FRotator, FTransform>;

    enum class EHistoryType
    {
        Transform,
        ObjectPresence
    };

    enum class EObjectType
    {
        StaticMesh,
        Camera
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
        EObjectType Type = EObjectType::StaticMesh;
        FTransform Transform;
        UStaticMesh* StaticMesh = nullptr;
        std::vector<FPropertySnapshot> CameraProperties;
    };

    struct FHistoryEntry
    {
        EHistoryType Type = EHistoryType::Transform;
        FObjectSnapshot Object;

        FTransform BeforeTransform;
        FTransform AfterTransform;

        bool bBeforePresent = true;
        bool bAfterPresent = true;

        uint32 BeforeSelectedUUID = 0;
        uint32 AfterSelectedUUID = 0;
    };

public:
    void Clear()
    {
        UndoStack.clear();
        RedoStack.clear();
    }

    bool CanUndo() const
    {
        return !UndoStack.empty();
    }

    bool CanRedo() const
    {
        return !RedoStack.empty();
    }

    void RecordCreate(const FEditorObjectContext& Context, UObject* Object, uint32 PreviousSelectedUUID)
    {
        (void)Context;
        FHistoryEntry Entry;

        if (Object == nullptr || !CaptureObject(Object, Entry.Object))
        {
            return;
        }

        Entry.Type = EHistoryType::ObjectPresence;
        Entry.bBeforePresent = false;
        Entry.bAfterPresent = true;
        Entry.BeforeSelectedUUID = PreviousSelectedUUID;
        Entry.AfterSelectedUUID = Object->GetUUID();

        PushEntry(std::move(Entry));
    }

    void RecordDelete(const FEditorObjectContext& Context, UObject* Object)
    {
        (void)Context;
        FHistoryEntry Entry;

        if (Object == nullptr || !CaptureObject(Object, Entry.Object))
        {
            return;
        }

        Entry.Type = EHistoryType::ObjectPresence;
        Entry.bBeforePresent = true;
        Entry.bAfterPresent = false;
        Entry.BeforeSelectedUUID = Object->GetUUID();
        Entry.AfterSelectedUUID = 0;

        PushEntry(std::move(Entry));
    }

    void RecordTransform(const FEditorObjectContext& Context, uint32 UUID, const FTransform& Before, const FTransform& After)
    {
        (void)Context;
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

    void Undo(FEditorObjectContext& Context)
    {
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

    static UObject* FindObject(const FEditorObjectContext& Context, uint32 UUID)
    {
        if (UUID == 0)
        {
            return nullptr;
        }

        for (UObject* Object : Context.Objects)
        {
            if (Object != nullptr && Object->GetUUID() == UUID)
            {
                return Object;
            }
        }

        return nullptr;
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

    static UObject* RestoreObject(const FObjectSnapshot& Snapshot)
    {
        UObject* NewObjectInstance = nullptr;

        if (Snapshot.Type == EObjectType::StaticMesh)
        {
            UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>();

            if (Component == nullptr || Snapshot.StaticMesh == nullptr)
            {
                delete Component;
                return nullptr;
            }

            Component->SetStaticMesh(Snapshot.StaticMesh);
            Component->SetTransform(Snapshot.Transform);
            NewObjectInstance = Component;
        }
        else if (Snapshot.Type == EObjectType::Camera)
        {
            UCameraComponent* Camera = NewObject<UCameraComponent>();

            if (Camera == nullptr)
            {
                return nullptr;
            }

            Camera->SetTransform(Snapshot.Transform);

            for (const FPropertySnapshot& PropertySnapshot : Snapshot.CameraProperties)
            {
                const FProperty* Property = Camera->GetClass()->FindProperty(PropertySnapshot.Name.c_str());

                if (Property == nullptr)
                {
                    delete Camera;
                    return nullptr;
                }

                const bool bApplied = std::visit([&](const auto& Value)
                {
                    return Property->SetValue(Camera, PropertySnapshot.Type, &Value);
                }, PropertySnapshot.Value);

                if (!bApplied)
                {
                    delete Camera;
                    return nullptr;
                }
            }

            NewObjectInstance = Camera;
        }

        if (NewObjectInstance == nullptr)
        {
            return nullptr;
        }

        NewObjectInstance->SetUUID(Snapshot.UUID);

        if (Snapshot.UUID < (std::numeric_limits<uint32>::max)() && FEngineStatics::NextUUID <= Snapshot.UUID)
        {
            FEngineStatics::NextUUID = Snapshot.UUID + 1;
        }

        return NewObjectInstance;
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

                Context.Objects.Add(RestoredObject);
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
};
