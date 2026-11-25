#include "Actors/CameraActor.h"
#include "Actors/StaticMeshActor.h"
#include "Core/Math/MathUtility.h"
#include "World/Level.h"
#include "World/World.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "EditorSceneSerializer.h"

#include "EditorObjectContext.h"

#include "Core/EngineStatics.h"
#include "Core/Logging/Log.h"

#include "Components/CameraComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Resources/AssetManager.h"
#include "Resources/PrimitiveMeshAssets.h"
#include "Resources/StaticMesh.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"
#include "TextureAssetImporter.h"

#include "UObject/Object.h"
#include "UObject/Class.h"

#include "Json/Dom/JsonObject.h"
#include "Json/Serialization/JsonSerializer.h"

#include "OBJStaticMeshImporter.h"
#include "Renderer/Renderer.h"
#include "EditorTransformGizmo.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <unordered_map>

extern TArray<UObject*> GUObjectArray;

namespace
{
    constexpr std::uintmax_t MaxSceneFileBytes = 64ull * 1024ull * 1024ull;
    
    enum class ESceneActorType
    {
        Actor,
        StaticMeshActor,
        CameraActor
    };

    enum class ESceneComponentType
    {
        Scene,
        StaticMesh,
        Camera
    };

    struct FSceneComponentRecord
    {
        uint32 UUID = 0;
        uint32 ParentUUID = 0;
        FName Name;
        ESceneComponentType Type = ESceneComponentType::Scene;
        FTransform RelativeTransform;
        UStaticMesh* StaticMesh = nullptr;
        UMaterial* Material = nullptr;
        TSharedPtr<FJsonObject> CameraSettings;
    };

    struct FSceneActorRecord
    {
        uint32 UUID = 0;
        uint32 RootComponentUUID = 0;

        FName Name;
        ESceneActorType Type = ESceneActorType::StaticMeshActor;

        FTransform Transform;

        UStaticMesh* StaticMesh = nullptr;
        UMaterial* Material = nullptr;
        TSharedPtr<FJsonObject> CameraSettings;
        bool bHasSerializedRoot = false;
        ESceneComponentType RootComponentType = ESceneComponentType::Scene;
        FName RootComponentName = FName("SceneRoot");
        FTransform RootRelativeTransform;
        UStaticMesh* RootStaticMesh = nullptr;
        UMaterial* RootMaterial = nullptr;
        TSharedPtr<FJsonObject> RootCameraSettings;
        TArray<FSceneComponentRecord> Components;
    };
    
    struct FScopedNextUUIDRollback
    {
        uint32 OriginalNextUUID = FEngineStatics::NextUUID;
        bool bCommitted = false;

        ~FScopedNextUUIDRollback()
        {
            if (!bCommitted)
            {
                FEngineStatics::NextUUID = OriginalNextUUID;
            }
        }
    };

    TArray<TSharedPtr<FJsonValue>> MakeNumberArray(double X, double Y, double Z)
    {
        TArray<TSharedPtr<FJsonValue>> Values;

        Values.Add(MakeShared<FJsonValueNumber>(X));
        Values.Add(MakeShared<FJsonValueNumber>(Y));
        Values.Add(MakeShared<FJsonValueNumber>(Z));

        return Values;
    }

    void WriteTransform(FJsonObject& JsonObject, const FTransform& Transform)
    {
        JsonObject.SetArrayField("Location", MakeNumberArray(Transform.Location.x, Transform.Location.y, Transform.Location.z));
        JsonObject.SetArrayField("Rotation", MakeNumberArray(Transform.Rotation.Pitch, Transform.Rotation.Yaw, Transform.Rotation.Roll));
        JsonObject.SetArrayField("Scale", MakeNumberArray(Transform.Scale3D.x, Transform.Scale3D.y, Transform.Scale3D.z));
    }
    
    bool WriteCameraSettings(FJsonObject& JsonObject, const UCameraComponent& Camera)
    {
        TArray<const FProperty*> Properties;
        Camera.GetClass()->GetProperties(Properties, true);

        FJsonObject Settings;

        for (const FProperty* Property : Properties)
        {
            if (Property->GetName().ToStd() == "Transform")
            {
                continue;
            }

            const void* ValuePtr = Property->GetValuePtr(&Camera);

            if (ValuePtr == nullptr)
            {
                return false;
            }

            switch (Property->GetType())
            {
            case EPropertyType::Bool:
                Settings.SetBoolField(Property->GetName(), *static_cast<const bool*>(ValuePtr));
                break;

            case EPropertyType::Float:
                Settings.SetNumberField(Property->GetName(), *static_cast<const float*>(ValuePtr));
                break;

            default:
                return false;
            }
        }

        JsonObject.SetObjectField("Settings", std::move(Settings));
        return true;
    }
    
    bool ValidateCameraSettings(const UCameraComponent& Camera, const FJsonObject* Settings)
    {
        if (Settings == nullptr)
        {
            return true;
        }

        TArray<const FProperty*> Properties;
        Camera.GetClass()->GetProperties(Properties, true);

        for (const FProperty* Property : Properties)
        {
            if (Property->GetName().ToStd() == "Transform")
            {
                continue;
            }

            const TSharedPtr<FJsonValue> Value = Settings->GetField(Property->GetName());

            if (!Value.IsValid())
            {
                continue;
            }

            switch (Property->GetType())
            {
            case EPropertyType::Bool:
                {
                    bool ConvertedValue = false;

                    if (!Value->TryGetBool(ConvertedValue))
                    {
                        return false;
                    }

                    break;
                }

            case EPropertyType::Float:
                {
                    double Number = 0.0;

                    if (!Value->TryGetNumber(Number) || !std::isfinite(Number) || std::fabs(Number) > (std::numeric_limits<float>::max)())
                    {
                        return false;
                    }

                    const std::string PropertyName = Property->GetName().ToStd();

                    if (PropertyName == "FieldOfView" && (Number <= 1.0 || Number >= 179.0))
                    {
                        return false;
                    }

                    if ((PropertyName == "AspectRatio" || PropertyName == "NearClipPlane" || PropertyName == "FarClipPlane" || PropertyName == "OrthoWidth") && Number <= 0.0)
                    {
                        return false;
                    }

                    if ((PropertyName == "MoveSpeed" || PropertyName == "MouseSensitivity" || PropertyName == "WheelSpeed") && Number < 0.0)
                    {
                        return false;
                    }

                    break;
                }

            default:
                return false;
            }
        }

        auto ReadEffectiveFloat = [&](const char* PropertyName, float& OutValue)
        {
            const FProperty* Property = Camera.GetClass()->FindProperty(PropertyName);

            if (Property == nullptr || Property->GetType() != EPropertyType::Float)
            {
                return false;
            }

            const void* CurrentValue = Property->GetValuePtr(&Camera);

            if (CurrentValue == nullptr)
            {
                return false;
            }

            OutValue = *static_cast<const float*>(CurrentValue);

            const TSharedPtr<FJsonValue> OverrideValue = Settings->GetField(PropertyName);

            if (OverrideValue.IsValid())
            {
                double Number = 0.0;

                if (!OverrideValue->TryGetNumber(Number) || !std::isfinite(Number) || std::fabs(Number) > (std::numeric_limits<float>::max)())
                {
                    return false;
                }

                OutValue = static_cast<float>(Number);
            }

            return std::isfinite(OutValue);
        };

        float NearClipPlane = 0.0f;
        float FarClipPlane = 0.0f;

        if (!ReadEffectiveFloat("NearClipPlane", NearClipPlane) || !ReadEffectiveFloat("FarClipPlane", FarClipPlane) || FarClipPlane <= NearClipPlane)
        {
            return false;
        }

        return true;
    }
    
    bool ApplyCameraSettings(UCameraComponent& Camera, const FJsonObject* Settings)
    {
        if (Settings == nullptr)
        {
            return true;
        }

        TArray<const FProperty*> Properties;
        Camera.GetClass()->GetProperties(Properties, true);

        for (const FProperty* Property : Properties)
        {
            if (Property->GetName().ToStd() == "Transform")
            {
                continue;
            }

            const TSharedPtr<FJsonValue> Value = Settings->GetField(Property->GetName());

            if (!Value.IsValid())
            {
                continue;
            }

            switch (Property->GetType())
            {
            case EPropertyType::Bool:
                {
                    bool ConvertedValue = false;

                    if (!Value->TryGetBool(ConvertedValue) || !Property->SetValue(&Camera, EPropertyType::Bool, &ConvertedValue))
                    {
                        return false;
                    }

                    break;
                }

            case EPropertyType::Float:
                {
                    double Number = 0.0;

                    if (!Value->TryGetNumber(Number) || !std::isfinite(Number) || std::fabs(Number) > (std::numeric_limits<float>::max)())
                    {
                        return false;
                    }

                    const float ConvertedValue = static_cast<float>(Number);

                    if (!Property->SetValue(&Camera, EPropertyType::Float, &ConvertedValue))
                    {
                        return false;
                    }

                    break;
                }

            default:
                return false;
            }
        }

        return true;
    }

    bool ReadNumberArray3(const FJsonObject& JsonObject, const FString& FieldName, double OutValues[3])
    {
        const TSharedPtr<FJsonValue> Value = JsonObject.GetField(FieldName, EJson::Array);

        if (!Value.IsValid())
        {
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>& Array = Value->AsArray();

        if (Array.Num() != 3)
        {
            return false;
        }

        for (int32 Index = 0; Index < 3; ++Index)
        {
            if (!Array[Index].IsValid() || !Array[Index]->TryGetNumber(OutValues[Index]) || !std::isfinite(OutValues[Index]) || std::fabs(OutValues[Index]) > (std::numeric_limits<float>::max)())
            {
                return false;
            }
        }

        return true;
    }

    bool ReadTransform(const FJsonObject& JsonObject, FTransform& OutTransform)
    {
        double Location[3] = {};
        double Rotation[3] = {};
        double Scale[3] = {};

        if (!ReadNumberArray3(JsonObject, "Location", Location) || !ReadNumberArray3(JsonObject, "Rotation", Rotation) || !ReadNumberArray3(JsonObject, "Scale", Scale))
        {
            return false;
        }

        OutTransform = FTransform(
            FVector(static_cast<float>(Location[0]), static_cast<float>(Location[1]), static_cast<float>(Location[2])),
            FRotator(static_cast<float>(Rotation[0]), static_cast<float>(Rotation[1]), static_cast<float>(Rotation[2])),
            FVector(static_cast<float>(Scale[0]), static_cast<float>(Scale[1]), static_cast<float>(Scale[2]))
        );

        if (std::fabs(OutTransform.Scale3D.x) <= EPSILON_1E_6 || std::fabs(OutTransform.Scale3D.y) <= EPSILON_1E_6 || std::fabs(OutTransform.Scale3D.z) <= EPSILON_1E_6)
        {
            return false;
        }

        return true;
    }

    bool TryParseUUID(const FString& Key, uint32& OutUUID)
    {
        const std::string& Text = Key.ToStd();

        if (Text.empty())
        {
            return false;
        }

        const char* Begin = Text.data();
        const char* End = Begin + Text.size();

        const auto Result = std::from_chars(Begin, End, OutUUID);

        return Result.ec == std::errc{} && Result.ptr == End && OutUUID != 0;
    }

    bool TryGetJsonObject(const TSharedPtr<FJsonValue>& Value, const FJsonObject*& OutObject)
    {
        OutObject = nullptr;

        if (!Value.IsValid())
        {
            return false;
        }

        const TSharedPtr<FJsonObject>* ObjectPtr = nullptr;

        if (!Value->TryGetObject(ObjectPtr) || ObjectPtr == nullptr || !ObjectPtr->IsValid())
        {
            return false;
        }

        OutObject = ObjectPtr->Get();
        return true;
    }

    bool HasUUIDCollision(uint32 UUID, const FEditorObjectContext& Context)
    {
        for (UObject* ExistingObject : GUObjectArray)
        {
            if (ExistingObject == nullptr)
            {
                continue;
            }

            if (Context.World != nullptr)
            {
                if (ExistingObject == Context.World || ExistingObject->GetTypedOuter<UWorld>() == Context.World)
                {
                    continue;
                }
            }

            if (ExistingObject->GetUUID() == UUID)
            {
                return true;
            }
        }

        return false;
    }

    bool ReadActorMap(const FJsonObject& ActorMap, double Version, FAssetManager& AssetManager, URenderer& Renderer, TArray<FSceneActorRecord>& OutRecords, TSet<uint32>& SeenUUIDs, uint32& OutMaxUUID)
    {
        for (const auto& Pair : ActorMap.GetValues())
        {
            FSceneActorRecord Record;

            if (!TryParseUUID(Pair.first, Record.UUID) || !SeenUUIDs.Add(Record.UUID))
            {
                return false;
            }

            const FJsonObject* ActorData = nullptr;

            if (!TryGetJsonObject(Pair.second, ActorData) || !ReadTransform(*ActorData, Record.Transform))
            {
                return false;
            }

            FString ClassName;
            FString ActorName;
            double RootComponentUUIDNumber = 0.0;

            if (!ActorData->TryGetStringField("Class", ClassName) || !ActorData->TryGetStringField("Name", ActorName) || ActorName.IsEmpty())
            {
                return false;
            }

            const bool bHasRootComponentUUID = ActorData->TryGetNumberField("RootComponentUUID", RootComponentUUIDNumber);

            if (!bHasRootComponentUUID || RootComponentUUIDNumber == 0.0)
            {
                Record.RootComponentUUID = 0;
            }
            else
            {
                if (!std::isfinite(RootComponentUUIDNumber) || RootComponentUUIDNumber < 1.0 || RootComponentUUIDNumber > (std::numeric_limits<uint32>::max)() || std::floor(RootComponentUUIDNumber) != RootComponentUUIDNumber)
                {
                    return false;
                }

                Record.RootComponentUUID = static_cast<uint32>(RootComponentUUIDNumber);

                if (!SeenUUIDs.Add(Record.RootComponentUUID))
                {
                    return false;
                }
            }

            if (ClassName == "Actor" && Record.RootComponentUUID != 0)
            {
                FString RootComponentClass;
                FString RootComponentName;
                if (Version < 3.0 || !ActorData->TryGetStringField("RootComponentClass", RootComponentClass)) return false;
                if (ActorData->TryGetStringField("RootComponentName", RootComponentName) && !RootComponentName.IsEmpty()) Record.RootComponentName = FName(RootComponentName);
                const TSharedPtr<FJsonValue> RootTransformValue = ActorData->GetField("RootComponentTransform");
                const FJsonObject* RootTransformObject = nullptr;
                if (!TryGetJsonObject(RootTransformValue, RootTransformObject) || !ReadTransform(*RootTransformObject, Record.RootRelativeTransform)) return false;
                if (RootComponentClass == "SceneComponent")
                {
                    Record.RootComponentType = ESceneComponentType::Scene;
                }
                else if (RootComponentClass == "StaticMeshComponent")
                {
                    Record.RootComponentType = ESceneComponentType::StaticMesh;
                    FString AssetPath;
                    if (ActorData->HasField("RootAssetPath"))
                    {
                        if (!ActorData->TryGetStringField("RootAssetPath", AssetPath) || AssetPath.IsEmpty()) return false;
                        Record.RootStaticMesh = OBJImporter::LoadOBJAsset(AssetPath, AssetManager, Renderer);
                        if (Record.RootStaticMesh == nullptr) return false;
                    }
                    FString MaterialAssetPath;
                    if (ActorData->HasField("RootMaterialAssetPath"))
                    {
                        if (!ActorData->TryGetStringField("RootMaterialAssetPath", MaterialAssetPath) || MaterialAssetPath.IsEmpty()) return false;
                        Record.RootMaterial = TextureImporter::LoadMaterialAsset(MaterialAssetPath, AssetManager, Renderer);
                        if (Record.RootMaterial == nullptr) return false;
                    }
                    else if (ActorData->HasField("RootTexturePath"))
                    {
                        FString TexturePath;
                        if (!ActorData->TryGetStringField("RootTexturePath", TexturePath) || TexturePath.IsEmpty()) return false;
                        Record.RootMaterial = TextureImporter::LoadTextureMaterial(TexturePath, AssetManager, Renderer);
                        if (Record.RootMaterial == nullptr) return false;
                    }
                }
                else if (RootComponentClass == "CameraComponent")
                {
                    Record.RootComponentType = ESceneComponentType::Camera;
                    if (ActorData->HasField("RootSettings"))
                    {
                        const TSharedPtr<FJsonValue> SettingsValue = ActorData->GetField("RootSettings");
                        const FJsonObject* SettingsObject = nullptr;
                        if (!TryGetJsonObject(SettingsValue, SettingsObject)) return false;
                        Record.RootCameraSettings = SettingsValue->AsObject();
                    }
                }
                else
                {
                    return false;
                }
                Record.bHasSerializedRoot = true;
            }

            if (ClassName != "Actor" && Record.RootComponentUUID == 0) return false;
            Record.Name = FName(ActorName);

            if (ClassName == "StaticMeshActor")
            {
                Record.Type = ESceneActorType::StaticMeshActor;

                FString AssetPath;

                if (!ActorData->TryGetStringField("AssetPath", AssetPath) || AssetPath.IsEmpty())
                {
                    return false;
                }

                Record.StaticMesh = OBJImporter::LoadOBJAsset(AssetPath, AssetManager, Renderer);

                if (Record.StaticMesh == nullptr)
                {
                    return false;
                }

                if (ActorData->HasField("MaterialAssetPath"))
                {
                    FString MaterialAssetPath;
                    if (!ActorData->TryGetStringField("MaterialAssetPath", MaterialAssetPath) || MaterialAssetPath.IsEmpty()) return false;
                    Record.Material = TextureImporter::LoadMaterialAsset(MaterialAssetPath, AssetManager, Renderer);
                    if (Record.Material == nullptr) return false;
                }
                else if (ActorData->HasField("TexturePath"))
                {
                    FString TexturePath;
                    if (!ActorData->TryGetStringField("TexturePath", TexturePath) || TexturePath.IsEmpty()) return false;
                    Record.Material = TextureImporter::LoadTextureMaterial(TexturePath, AssetManager, Renderer);
                    if (Record.Material == nullptr) return false;
                }
            }
            else if (ClassName == "Actor")
            {
                Record.Type = ESceneActorType::Actor;
            }
            else if (ClassName == "CameraActor")
            {
                Record.Type = ESceneActorType::CameraActor;

                if (ActorData->HasField("Settings"))
                {
                    const TSharedPtr<FJsonValue> SettingsValue = ActorData->GetField("Settings");
                    const FJsonObject* SettingsObject = nullptr;

                    if (!TryGetJsonObject(SettingsValue, SettingsObject))
                    {
                        return false;
                    }

                    Record.CameraSettings = SettingsValue->AsObject();
                }
            }
            else
            {
                return false;
            }

            if (Version >= 3.0)
            {
                const TArray<TSharedPtr<FJsonValue>>* ComponentValues = nullptr;
                if (!ActorData->TryGetArrayField("Components", ComponentValues) || ComponentValues == nullptr) return false;
                for (const TSharedPtr<FJsonValue>& ComponentValue : *ComponentValues)
                {
                    const FJsonObject* ComponentData = nullptr;
                    if (!TryGetJsonObject(ComponentValue, ComponentData)) return false;
                    FSceneComponentRecord ComponentRecord;
                    double ComponentUUIDNumber = 0.0;
                    double ParentUUIDNumber = 0.0;
                    FString ComponentClass;
                    FString ComponentName;
                    if (!ComponentData->TryGetNumberField("UUID", ComponentUUIDNumber) || !std::isfinite(ComponentUUIDNumber) || ComponentUUIDNumber < 1.0 || ComponentUUIDNumber > (std::numeric_limits<uint32>::max)() || std::floor(ComponentUUIDNumber) != ComponentUUIDNumber) return false;
                    ComponentRecord.UUID = static_cast<uint32>(ComponentUUIDNumber);
                    if (!SeenUUIDs.Add(ComponentRecord.UUID) || !ComponentData->TryGetStringField("Class", ComponentClass) || !ComponentData->TryGetStringField("Name", ComponentName) || ComponentName.IsEmpty()) return false;
                    if (ComponentData->HasField("ParentUUID"))
                    {
                        if (!ComponentData->TryGetNumberField("ParentUUID", ParentUUIDNumber) || !std::isfinite(ParentUUIDNumber) || ParentUUIDNumber < 0.0 || ParentUUIDNumber > (std::numeric_limits<uint32>::max)() || std::floor(ParentUUIDNumber) != ParentUUIDNumber) return false;
                    }
                    ComponentRecord.ParentUUID = static_cast<uint32>(ParentUUIDNumber);
                    ComponentRecord.Name = FName(ComponentName);
                    const TSharedPtr<FJsonValue> RelativeTransformValue = ComponentData->GetField("RelativeTransform");
                    const FJsonObject* RelativeTransformObject = nullptr;
                    if (!TryGetJsonObject(RelativeTransformValue, RelativeTransformObject) || !ReadTransform(*RelativeTransformObject, ComponentRecord.RelativeTransform)) return false;

                    if (ComponentClass == "SceneComponent")
                    {
                        ComponentRecord.Type = ESceneComponentType::Scene;
                    }
                    else if (ComponentClass == "StaticMeshComponent")
                    {
                        ComponentRecord.Type = ESceneComponentType::StaticMesh;
                        FString AssetPath;
                        if (ComponentData->HasField("AssetPath"))
                        {
                            if (!ComponentData->TryGetStringField("AssetPath", AssetPath) || AssetPath.IsEmpty()) return false;
                            ComponentRecord.StaticMesh = OBJImporter::LoadOBJAsset(AssetPath, AssetManager, Renderer);
                            if (ComponentRecord.StaticMesh == nullptr) return false;
                        }
                        if (ComponentData->HasField("MaterialAssetPath"))
                        {
                            FString MaterialAssetPath;
                            if (!ComponentData->TryGetStringField("MaterialAssetPath", MaterialAssetPath) || MaterialAssetPath.IsEmpty()) return false;
                            ComponentRecord.Material = TextureImporter::LoadMaterialAsset(MaterialAssetPath, AssetManager, Renderer);
                            if (ComponentRecord.Material == nullptr) return false;
                        }
                        else if (ComponentData->HasField("TexturePath"))
                        {
                            FString TexturePath;
                            if (!ComponentData->TryGetStringField("TexturePath", TexturePath) || TexturePath.IsEmpty()) return false;
                            ComponentRecord.Material = TextureImporter::LoadTextureMaterial(TexturePath, AssetManager, Renderer);
                            if (ComponentRecord.Material == nullptr) return false;
                        }
                    }
                    else if (ComponentClass == "CameraComponent")
                    {
                        ComponentRecord.Type = ESceneComponentType::Camera;
                        if (ComponentData->HasField("Settings"))
                        {
                            const TSharedPtr<FJsonValue> SettingsValue = ComponentData->GetField("Settings");
                            const FJsonObject* SettingsObject = nullptr;
                            if (!TryGetJsonObject(SettingsValue, SettingsObject)) return false;
                            ComponentRecord.CameraSettings = SettingsValue->AsObject();
                        }
                    }
                    else
                    {
                        return false;
                    }
                    if (ComponentRecord.ParentUUID == ComponentRecord.UUID || ComponentRecord.ParentUUID == Record.UUID) return false;
                    OutMaxUUID = (std::max)(OutMaxUUID, ComponentRecord.UUID);
                    Record.Components.Add(std::move(ComponentRecord));
                }

                if (Record.RootComponentUUID == 0 && (!Record.Components.IsEmpty() || Record.bHasSerializedRoot)) return false;
                for (const FSceneComponentRecord& ComponentRecord : Record.Components)
                {
                    if (ComponentRecord.ParentUUID != 0 && ComponentRecord.ParentUUID != Record.RootComponentUUID)
                    {
                        bool bParentFound = false;
                        for (const FSceneComponentRecord& Candidate : Record.Components) bParentFound = bParentFound || Candidate.UUID == ComponentRecord.ParentUUID;
                        if (!bParentFound) return false;
                    }
                }
                for (const FSceneComponentRecord& ComponentRecord : Record.Components)
                {
                    uint32 ParentUUID = ComponentRecord.ParentUUID;
                    TSet<uint32> VisitedParents;
                    while (ParentUUID != 0 && ParentUUID != Record.RootComponentUUID)
                    {
                        if (!VisitedParents.Add(ParentUUID)) return false;
                        const FSceneComponentRecord* ParentRecord = nullptr;
                        for (const FSceneComponentRecord& Candidate : Record.Components) if (Candidate.UUID == ParentUUID) { ParentRecord = &Candidate; break; }
                        if (ParentRecord == nullptr) return false;
                        ParentUUID = ParentRecord->ParentUUID;
                    }
                }
            }

            OutMaxUUID = (std::max)(OutMaxUUID, Record.UUID);

            if (Record.RootComponentUUID != 0)
            {
                OutMaxUUID = (std::max)(OutMaxUUID, Record.RootComponentUUID);
            }

            OutRecords.Add(std::move(Record));
        }

        return true;
    }
    
    bool ReadLegacyObjectMap(const FJsonObject& ObjectMap, bool bIsCamera, FAssetManager& AssetManager, URenderer& Renderer, TArray<FSceneActorRecord>& OutRecords, TSet<uint32>& SeenUUIDs, uint32& OutMaxUUID)
    {
        for (const auto& Pair : ObjectMap.GetValues())
        {
            FSceneActorRecord Record;

            if (!TryParseUUID(Pair.first, Record.UUID) || !SeenUUIDs.Add(Record.UUID))
            {
                return false;
            }

            const FJsonObject* ObjectData = nullptr;

            if (!TryGetJsonObject(Pair.second, ObjectData) || !ReadTransform(*ObjectData, Record.Transform))
            {
                return false;
            }

            Record.Name = FName(bIsCamera ? "CameraActor" : "StaticMeshActor");
            Record.RootComponentUUID = 0;

            if (bIsCamera)
            {
                Record.Type = ESceneActorType::CameraActor;

                if (ObjectData->HasField("Settings"))
                {
                    const TSharedPtr<FJsonValue> SettingsValue = ObjectData->GetField("Settings");
                    const FJsonObject* SettingsObject = nullptr;

                    if (!TryGetJsonObject(SettingsValue, SettingsObject))
                    {
                        return false;
                    }

                    Record.CameraSettings = SettingsValue->AsObject();
                }
            }
            else
            {
                Record.Type = ESceneActorType::StaticMeshActor;

                FString AssetPath;

                if (!ObjectData->TryGetStringField("AssetPath", AssetPath) || AssetPath.IsEmpty())
                {
                    return false;
                }

                Record.StaticMesh = OBJImporter::LoadOBJAsset(AssetPath, AssetManager, Renderer);

                if (Record.StaticMesh == nullptr)
                {
                    return false;
                }

                if (ObjectData->HasField("MaterialAssetPath"))
                {
                    FString MaterialAssetPath;
                    if (!ObjectData->TryGetStringField("MaterialAssetPath", MaterialAssetPath) || MaterialAssetPath.IsEmpty()) return false;
                    Record.Material = TextureImporter::LoadMaterialAsset(MaterialAssetPath, AssetManager, Renderer);
                    if (Record.Material == nullptr) return false;
                }
                else if (ObjectData->HasField("TexturePath"))
                {
                    FString TexturePath;
                    if (!ObjectData->TryGetStringField("TexturePath", TexturePath) || TexturePath.IsEmpty()) return false;
                    Record.Material = TextureImporter::LoadTextureMaterial(TexturePath, AssetManager, Renderer);
                    if (Record.Material == nullptr) return false;
                }
            }

            OutMaxUUID = (std::max)(OutMaxUUID, Record.UUID);
            OutRecords.Add(std::move(Record));
        }

        return true;
    }
    
    bool CopyCameraSettings(const UCameraComponent& Source, UCameraComponent& Destination)
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
}

bool FSceneSerializer::Save(const std::filesystem::path& FilePath, const FEditorObjectContext& Context, const UCameraComponent& ViewportCamera)
{
    if (FilePath.empty())
    {
        return false;
    }

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();

    ULevel* Level = Context.World != nullptr ? Context.World->GetPersistentLevel() : nullptr;

    if (Level == nullptr)
    {
        return false;
    }

    TSharedPtr<FJsonObject> Actors = MakeShared<FJsonObject>();

    Root->SetNumberField("Version", 3);
    Root->SetNumberField("NextUUID", FEngineStatics::NextUUID);

    for (AActor* Actor : Level->GetActors())
    {
        if (Actor == nullptr)
        {
            continue;
        }

        FJsonObject ActorData;
        USceneComponent* RootComponent = Actor->GetRootComponent();

        if (RootComponent == nullptr && !Actor->GetComponents().IsEmpty())
        {
            UE_LOG(Error, "Scene Save failed: Actor has components but no root component");
            return false;
        }

        WriteTransform(ActorData, Actor->GetActorTransform());
        ActorData.SetStringField("Name", Actor->GetName());

        if (RootComponent != nullptr)
        {
            ActorData.SetNumberField("RootComponentUUID", RootComponent->GetUUID());
        }

        if (AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor))
        {
            UStaticMeshComponent* Component = StaticMeshActor->GetStaticMeshComponent();

            if (Component == nullptr || Component->GetStaticMesh() == nullptr)
            {
                return false;
            }

            const FString& AssetPath = Component->GetStaticMesh()->GetAssetPath();

            if (AssetPath.IsEmpty())
            {
                return false;
            }

            ActorData.SetStringField("Class", "StaticMeshActor");
            ActorData.SetStringField("AssetPath", AssetPath);

            UMaterial* Material = Component->GetMaterial(0);

            if (Material != nullptr)
            {
                const FString& MaterialAssetPath = Material->GetAssetPath();
                if (MaterialAssetPath.IsEmpty()) return false;
                ActorData.SetStringField("MaterialAssetPath", MaterialAssetPath);

                if (Material->GetBaseColorTexture() != nullptr)
                {
                    const FString& TexturePath = Material->GetBaseColorTexture()->GetAssetPath();
                    if (TexturePath.IsEmpty()) return false;
                    ActorData.SetStringField("TexturePath", TexturePath);
                }
            }
        }
        else if (ACameraActor* CameraActor = Cast<ACameraActor>(Actor))
        {
            UCameraComponent* Camera = CameraActor->GetCameraComponent();

            if (Camera == nullptr)
            {
                return false;
            }

            ActorData.SetStringField("Class", "CameraActor");

            if (!WriteCameraSettings(ActorData, *Camera))
            {
                return false;
            }
        }
        else if (Actor->GetClass() == AActor::StaticClass())
        {
            ActorData.SetStringField("Class", "Actor");
            if (RootComponent != nullptr)
            {
                FString RootComponentClass = "SceneComponent";
                if (UStaticMeshComponent* RootMesh = Cast<UStaticMeshComponent>(RootComponent))
                {
                    RootComponentClass = "StaticMeshComponent";
                    if (RootMesh->GetStaticMesh() != nullptr)
                    {
                        const FString& AssetPath = RootMesh->GetStaticMesh()->GetAssetPath();
                        if (AssetPath.IsEmpty()) return false;
                        ActorData.SetStringField("RootAssetPath", AssetPath);
                    }
                    UMaterial* RootMaterial = RootMesh->HasMaterialOverride(0) ? RootMesh->GetMaterial(0) : nullptr;

                    if (RootMaterial != nullptr)
                    {
                        const FString& MaterialAssetPath = RootMaterial->GetAssetPath();
                        if (MaterialAssetPath.IsEmpty()) return false;
                        ActorData.SetStringField("RootMaterialAssetPath", MaterialAssetPath);

                        if (RootMaterial->GetBaseColorTexture() != nullptr)
                        {
                            const FString& TexturePath = RootMaterial->GetBaseColorTexture()->GetAssetPath();
                            if (TexturePath.IsEmpty()) return false;
                            ActorData.SetStringField("RootTexturePath", TexturePath);
                        }
                    }
                }
                else if (UCameraComponent* RootCamera = Cast<UCameraComponent>(RootComponent))
                {
                    RootComponentClass = "CameraComponent";
                    FJsonObject RootCameraData;
                    if (!WriteCameraSettings(RootCameraData, *RootCamera)) return false;
                    const TSharedPtr<FJsonValue> SettingsValue = RootCameraData.GetField("Settings");
                    if (!SettingsValue.IsValid()) return false;
                    ActorData.SetField("RootSettings", SettingsValue);
                }
                ActorData.SetStringField("RootComponentClass", RootComponentClass);
                ActorData.SetStringField("RootComponentName", RootComponent->GetName());
                FJsonObject RootComponentData;
                WriteTransform(RootComponentData, RootComponent->GetRelativeTransform());
                ActorData.SetObjectField("RootComponentTransform", std::move(RootComponentData));
            }
        }
        else
        {
            UE_LOG(Error, "Scene Save failed: Unsupported Actor class");
            return false;
        }

        TArray<TSharedPtr<FJsonValue>> ComponentValues;
        for (UActorComponent* ActorComponent : Actor->GetComponents())
        {
            if (ActorComponent == nullptr || ActorComponent == RootComponent) continue;
            USceneComponent* SceneComponent = Cast<USceneComponent>(ActorComponent);
            if (SceneComponent == nullptr) return false;

            FJsonObject ComponentData;
            ComponentData.SetNumberField("UUID", ActorComponent->GetUUID());
            ComponentData.SetStringField("Name", ActorComponent->GetName());
            ComponentData.SetNumberField("ParentUUID", SceneComponent->GetAttachParent() != nullptr ? SceneComponent->GetAttachParent()->GetUUID() : 0);
            WriteTransform(ComponentData, SceneComponent->GetRelativeTransform());

            if (UCameraComponent* CameraComponent = Cast<UCameraComponent>(ActorComponent))
            {
                ComponentData.SetStringField("Class", "CameraComponent");
                if (!WriteCameraSettings(ComponentData, *CameraComponent)) return false;
            }
            else if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(ActorComponent))
            {
                ComponentData.SetStringField("Class", "StaticMeshComponent");
                if (MeshComponent->GetStaticMesh() != nullptr)
                {
                    const FString& AssetPath = MeshComponent->GetStaticMesh()->GetAssetPath();
                    if (AssetPath.IsEmpty()) return false;
                    ComponentData.SetStringField("AssetPath", AssetPath);
                }
                UMaterial* Material = MeshComponent->HasMaterialOverride(0) ? MeshComponent->GetMaterial(0) : nullptr;

                if (Material != nullptr)
                {
                    const FString& MaterialAssetPath = Material->GetAssetPath();
                    if (MaterialAssetPath.IsEmpty()) return false;
                    ComponentData.SetStringField("MaterialAssetPath", MaterialAssetPath);

                    if (Material->GetBaseColorTexture() != nullptr)
                    {
                        const FString& TexturePath = Material->GetBaseColorTexture()->GetAssetPath();
                        if (TexturePath.IsEmpty()) return false;
                        ComponentData.SetStringField("TexturePath", TexturePath);
                    }
                }
            }
            else
            {
                ComponentData.SetStringField("Class", "SceneComponent");
            }

            ComponentValues.Add(MakeShared<FJsonValueObject>(MakeShared<FJsonObject>(std::move(ComponentData))));
        }
        ActorData.SetArrayField("Components", std::move(ComponentValues));

        Actors->SetObjectField(std::to_string(Actor->GetUUID()).c_str(), std::move(ActorData));
    }

    Root->SetObjectField("Actors", std::move(*Actors));

    FJsonObject ViewportCameraData;
    WriteTransform(ViewportCameraData, ViewportCamera.GetTransform());

    if (!WriteCameraSettings(ViewportCameraData, ViewportCamera))
    {
        UE_LOG(Error, "Scene Save failed: Unsupported viewport camera property type");
        return false;
    }

    Root->SetObjectField("ViewportCamera", std::move(ViewportCameraData));

    FString JsonString;
    FString SerializeError;

    if (!FJsonSerializer::Serialize(Root, JsonString, true, &SerializeError))
    {
        UE_LOG(Error, "Scene Save failed: %s", SerializeError.c_str());
        return false;
    }

    std::filesystem::path TemporaryPath = FilePath;
    TemporaryPath += L".tmp";

    auto RemoveTemporaryFile = [&TemporaryPath]()
    {
        std::error_code RemoveError;
        std::filesystem::remove(TemporaryPath, RemoveError);
    };

    std::ofstream OutputFile(TemporaryPath, std::ios::binary | std::ios::out | std::ios::trunc);

    if (!OutputFile.is_open())
    {
        UE_LOG(Error, "Scene Save failed: Cannot open temporary file");
        return false;
    }

    const std::string& JsonText = JsonString.ToStd();

    OutputFile.write(JsonText.data(), static_cast<std::streamsize>(JsonText.size()));
    OutputFile.flush();

    if (!OutputFile.good())
    {
        OutputFile.close();
        RemoveTemporaryFile();
        UE_LOG(Error, "Scene Save failed: File write error");
        return false;
    }

    OutputFile.close();

    if (OutputFile.fail())
    {
        RemoveTemporaryFile();
        UE_LOG(Error, "Scene Save failed: File close error");
        return false;
    }

    if (!MoveFileExW(TemporaryPath.c_str(), FilePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        const DWORD ErrorCode = GetLastError();
        RemoveTemporaryFile();
        UE_LOG(Error, "Scene Save failed: Cannot replace target file (%u)", static_cast<uint32>(ErrorCode));
        return false;
    }

    UE_LOG(Info, "Scene saved: %s", FilePath.string().c_str());
    return true;
}

bool FSceneSerializer::Load(const std::filesystem::path& FilePath, FEditorObjectContext& Context, FAssetManager& AssetManager, UCameraComponent& ViewportCamera, URenderer& Renderer)
{
    if (FilePath.empty())
    {
        return false;
    }

    std::ifstream InputFile(FilePath, std::ios::binary);

    if (!InputFile.is_open())
    {
        UE_LOG(Error, "Scene Load failed: Cannot open input file");
        return false;
    }

    std::error_code FileSizeError;
    const std::uintmax_t FileSize = std::filesystem::file_size(FilePath, FileSizeError);

    if (FileSizeError)
    {
        UE_LOG(Error, "Scene Load failed: Cannot query input file size");
        return false;
    }

    if (FileSize > MaxSceneFileBytes)
    {
        UE_LOG(Error, "Scene Load failed: File exceeds maximum size");
        return false;
    }

    std::string JsonText((std::istreambuf_iterator<char>(InputFile)), std::istreambuf_iterator<char>());

    if (InputFile.bad())
    {
        UE_LOG(Error, "Scene Load failed: File read error");
        return false;
    }

    if (JsonText.size() > MaxSceneFileBytes)
    {
        UE_LOG(Error, "Scene Load failed: File exceeds maximum size");
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    FString DeserializeError;

    if (!FJsonSerializer::DeserializeObject(FString(std::move(JsonText)), Root, &DeserializeError))
    {
        UE_LOG(Error, "Scene Load failed: %s", DeserializeError.c_str());
        return false;
    }

    double Version = 0.0;
    double NextUUIDNumber = 0.0;

    if (!Root->TryGetNumberField("Version", Version) || (Version != 1.0 && Version != 2.0 && Version != 3.0) || !Root->TryGetNumberField("NextUUID", NextUUIDNumber))
    {
        UE_LOG(Error, "Scene Load failed: Invalid scene header");
        return false;
    }

    if (!std::isfinite(NextUUIDNumber) || NextUUIDNumber < 1.0 || NextUUIDNumber > (std::numeric_limits<uint32>::max)() || std::floor(NextUUIDNumber) != NextUUIDNumber)
    {
        UE_LOG(Error, "Scene Load failed: Invalid NextUUID");
        return false;
    }
    
    bool bHasViewportCameraData = false;
    FTransform ViewportCameraTransform;
    TSharedPtr<FJsonObject> ViewportCameraSettings;

    const TSharedPtr<FJsonValue> ViewportCameraValue = Root->GetField("ViewportCamera");

    if (ViewportCameraValue.IsValid())
    {
        const FJsonObject* ViewportCameraObject = nullptr;

        if (!TryGetJsonObject(ViewportCameraValue, ViewportCameraObject) || !ReadTransform(*ViewportCameraObject, ViewportCameraTransform))
        {
            UE_LOG(Error, "Scene Load failed: Invalid viewport camera data");
            return false;
        }

        if (ViewportCameraObject->HasField("Settings"))
        {
            const TSharedPtr<FJsonValue> SettingsValue = ViewportCameraObject->GetField("Settings");
            const FJsonObject* SettingsObject = nullptr;

            if (!TryGetJsonObject(SettingsValue, SettingsObject))
            {
                UE_LOG(Error, "Scene Load failed: Invalid viewport camera settings");
                return false;
            }

            ViewportCameraSettings = SettingsValue->AsObject();
        }

        if (!ValidateCameraSettings(ViewportCamera, ViewportCameraSettings.Get()))
        {
            UE_LOG(Error, "Scene Load failed: Invalid viewport camera property values");
            return false;
        }

        bHasViewportCameraData = true;
    }
    
    TArray<FSceneActorRecord> Records;
    TSet<uint32> SeenUUIDs;
    uint32 MaxUUID = 0;

    if (Version >= 2.0)
    {
        const TSharedPtr<FJsonValue> ActorsValue = Root->GetField("Actors", EJson::Object);
        const FJsonObject* ActorsObject = nullptr;

        if (!TryGetJsonObject(ActorsValue, ActorsObject))
        {
            UE_LOG(Error, "Scene Load failed: Actors must be an object");
            return false;
        }

        if (!ReadActorMap(*ActorsObject, Version, AssetManager, Renderer, Records, SeenUUIDs, MaxUUID))
        {
            UE_LOG(Error, "Scene Load failed: Invalid actor data");
            return false;
        }
    }
    else
    {
        const TSharedPtr<FJsonValue> PrimitiveValue = Root->GetField("Primitives", EJson::Object);
        const FJsonObject* PrimitiveObject = nullptr;

        if (!TryGetJsonObject(PrimitiveValue, PrimitiveObject))
        {
            UE_LOG(Error, "Scene Load failed: Primitives must be an object");
            return false;
        }

        const TSharedPtr<FJsonValue> CameraValue = Root->GetField("Cameras");
        const FJsonObject* CameraObject = nullptr;

        if (CameraValue.IsValid() && !TryGetJsonObject(CameraValue, CameraObject))
        {
            UE_LOG(Error, "Scene Load failed: Cameras must be an object");
            return false;
        }

        if (!ReadLegacyObjectMap(*PrimitiveObject, false, AssetManager, Renderer, Records, SeenUUIDs, MaxUUID))
        {
            UE_LOG(Error, "Scene Load failed: Invalid legacy primitive data");
            return false;
        }

        if (CameraObject != nullptr && !ReadLegacyObjectMap(*CameraObject, true, AssetManager, Renderer, Records, SeenUUIDs, MaxUUID))
        {
            UE_LOG(Error, "Scene Load failed: Invalid legacy camera data");
            return false;
        }
    }

    if (MaxUUID == (std::numeric_limits<uint32>::max)())
    {
        UE_LOG(Error, "Scene Load failed: UUID range exhausted");
        return false;
    }

    uint32 SavedNextUUID = static_cast<uint32>(NextUUIDNumber);

    if (SavedNextUUID <= MaxUUID)
    {
        SavedNextUUID = MaxUUID + 1;
    }

    for (const FSceneActorRecord& Record : Records)
    {
        if (HasUUIDCollision(Record.UUID, Context) || (Record.RootComponentUUID != 0 && HasUUIDCollision(Record.RootComponentUUID, Context)))
        {
            UE_LOG(Error, "Scene Load failed: UUID collision (Actor=%u, RootComponent=%u)", Record.UUID, Record.RootComponentUUID);
            return false;
        }
        for (const FSceneComponentRecord& ComponentRecord : Record.Components)
        {
            if (HasUUIDCollision(ComponentRecord.UUID, Context))
            {
                UE_LOG(Error, "Scene Load failed: Component UUID collision (%u)", ComponentRecord.UUID);
                return false;
            }
        }
    }
    
    const uint32 StagingStartUUID = (std::max)(FEngineStatics::NextUUID, SavedNextUUID);

    uint64 TotalComponentCount = 0;
    for (const FSceneActorRecord& Record : Records) TotalComponentCount += (Record.RootComponentUUID != 0 ? 1ull : 0ull) + static_cast<uint64>(Record.Components.Num());
    const uint64 RequiredUUIDCount = 2ull + static_cast<uint64>(Records.Num()) + TotalComponentCount + (bHasViewportCameraData ? 1ull : 0ull);

    if (StagingStartUUID == 0 || static_cast<uint64>(StagingStartUUID) + RequiredUUIDCount > (std::numeric_limits<uint32>::max)())
    {
        UE_LOG(Error, "Scene Load failed: Runtime UUID range exhausted");
        return false;
    }

    FScopedNextUUIDRollback UUIDRollback;

    FEngineStatics::NextUUID = StagingStartUUID;

    std::unique_ptr<UWorld> StagedWorld(NewObject<UWorld>(nullptr, FName("StagedSceneWorld")));

    if (StagedWorld == nullptr || StagedWorld->GetPersistentLevel() == nullptr)
    {
        return false;
    }

    for (const FSceneActorRecord& Record : Records)
    {
        AActor* NewActor = nullptr;

        if (Record.Type == ESceneActorType::StaticMeshActor)
        {
            AStaticMeshActor* StaticMeshActor = StagedWorld->SpawnActor<AStaticMeshActor>(Record.Name, Record.Transform);

            if (StaticMeshActor == nullptr || StaticMeshActor->GetStaticMeshComponent() == nullptr)
            {
                UE_LOG(Error, "Scene Load failed: StaticMeshActor construction failed");
                return false;
            }

            StaticMeshActor->GetStaticMeshComponent()->SetStaticMesh(Record.StaticMesh);
            if (Record.Material != nullptr) StaticMeshActor->GetStaticMeshComponent()->SetMaterial(0, Record.Material);

            NewActor = StaticMeshActor;
        }
        else if (Record.Type == ESceneActorType::Actor)
        {
            NewActor = StagedWorld->SpawnActor<AActor>(Record.Name, Record.Transform);
        }
        else if (Record.Type == ESceneActorType::CameraActor)
        {
            ACameraActor* CameraActor = StagedWorld->SpawnActor<ACameraActor>(Record.Name, Record.Transform);

            if (CameraActor == nullptr || CameraActor->GetCameraComponent() == nullptr)
            {
                UE_LOG(Error, "Scene Load failed: CameraActor construction failed");
                return false;
            }

            UCameraComponent* CameraComponent = CameraActor->GetCameraComponent();

            if (CameraComponent == nullptr || !ValidateCameraSettings(*CameraComponent, Record.CameraSettings.Get()) || !ApplyCameraSettings(*CameraComponent, Record.CameraSettings.Get()))
            {
                UE_LOG(Error, "Scene Load failed: Invalid CameraActor settings");
                return false;
            }

            NewActor = CameraActor;
        }

        if (NewActor == nullptr)
        {
            UE_LOG(Error, "Scene Load failed: Unsupported Actor type");
            return false;
        }

        USceneComponent* RootComponent = NewActor->GetRootComponent();

        if (Record.RootComponentUUID != 0 && RootComponent == nullptr && !Record.bHasSerializedRoot)
        {
            UE_LOG(Error, "Scene Load failed: Actor has no Root Component");
            return false;
        }

        NewActor->SetUUID(Record.UUID);

        if (RootComponent != nullptr && Record.RootComponentUUID != 0)
        {
            RootComponent->SetUUID(Record.RootComponentUUID);
        }
        if (Record.bHasSerializedRoot)
        {
            if (RootComponent != nullptr) return false;
            switch (Record.RootComponentType)
            {
            case ESceneComponentType::Scene: RootComponent = NewActor->CreateComponent<USceneComponent>(Record.RootComponentName); break;
            case ESceneComponentType::StaticMesh: RootComponent = NewActor->CreateComponent<UStaticMeshComponent>(Record.RootComponentName); break;
            case ESceneComponentType::Camera: RootComponent = NewActor->CreateComponent<UCameraComponent>(Record.RootComponentName); break;
            default: return false;
            }
            if (RootComponent == nullptr || !NewActor->SetRootComponent(RootComponent)) return false;
            RootComponent->SetUUID(Record.RootComponentUUID);
            RootComponent->SetRelativeTransform(Record.RootRelativeTransform);
            if (UStaticMeshComponent* RootMesh = Cast<UStaticMeshComponent>(RootComponent))
            {
                RootMesh->SetStaticMesh(Record.RootStaticMesh);
                if (Record.RootMaterial != nullptr) RootMesh->SetMaterial(0, Record.RootMaterial);
            }
            if (UCameraComponent* RootCamera = Cast<UCameraComponent>(RootComponent))
            {
                if (!ValidateCameraSettings(*RootCamera, Record.RootCameraSettings.Get()) || !ApplyCameraSettings(*RootCamera, Record.RootCameraSettings.Get())) return false;
            }
        }

        std::unordered_map<uint32, USceneComponent*> ComponentMap;
        if (RootComponent != nullptr) ComponentMap.emplace(RootComponent->GetUUID(), RootComponent);
        for (const FSceneComponentRecord& ComponentRecord : Record.Components)
        {
            UActorComponent* RestoredComponent = nullptr;
            switch (ComponentRecord.Type)
            {
            case ESceneComponentType::Scene: RestoredComponent = NewActor->CreateComponent<USceneComponent>(ComponentRecord.Name); break;
            case ESceneComponentType::StaticMesh: RestoredComponent = NewActor->CreateComponent<UStaticMeshComponent>(ComponentRecord.Name); break;
            case ESceneComponentType::Camera: RestoredComponent = NewActor->CreateComponent<UCameraComponent>(ComponentRecord.Name); break;
            default: return false;
            }
            if (RestoredComponent == nullptr) return false;
            RestoredComponent->SetUUID(ComponentRecord.UUID);
            if (RestoredComponent->GetFName() != ComponentRecord.Name) return false;
            USceneComponent* RestoredSceneComponent = Cast<USceneComponent>(RestoredComponent);
            if (RestoredSceneComponent == nullptr) return false;
            RestoredSceneComponent->SetRelativeTransform(ComponentRecord.RelativeTransform);
            if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(RestoredComponent))
            {
                MeshComponent->SetStaticMesh(ComponentRecord.StaticMesh);
                if (ComponentRecord.Material != nullptr) MeshComponent->SetMaterial(0, ComponentRecord.Material);
            }
            if (UCameraComponent* CameraComponent = Cast<UCameraComponent>(RestoredComponent))
            {
                if (!ValidateCameraSettings(*CameraComponent, ComponentRecord.CameraSettings.Get()) || !ApplyCameraSettings(*CameraComponent, ComponentRecord.CameraSettings.Get())) return false;
            }
            ComponentMap.emplace(ComponentRecord.UUID, RestoredSceneComponent);
        }
        for (const FSceneComponentRecord& ComponentRecord : Record.Components)
        {
            if (ComponentRecord.ParentUUID == 0) continue;
            const auto Child = ComponentMap.find(ComponentRecord.UUID);
            const auto Parent = ComponentMap.find(ComponentRecord.ParentUUID);
            if (Child == ComponentMap.end() || Parent == ComponentMap.end() || !Child->second->AttachToComponent(Parent->second, false)) return false;
        }
    }
    
    std::unique_ptr<UCameraComponent> StagedViewportCamera;

    if (bHasViewportCameraData)
    {
        StagedViewportCamera.reset(NewObject<UCameraComponent>());

        if (StagedViewportCamera == nullptr)
        {
            UE_LOG(Error, "Scene Load failed: Cannot create staged viewport camera");
            return false;
        }

        if (!CopyCameraSettings(ViewportCamera, *StagedViewportCamera))
        {
            UE_LOG(Error, "Scene Load failed: Cannot copy viewport camera settings");
            return false;
        }

        StagedViewportCamera->SetTransform(ViewportCameraTransform);

        if (!ApplyCameraSettings(*StagedViewportCamera, ViewportCameraSettings.Get()))
        {
            UE_LOG(Error, "Scene Load failed: Cannot apply viewport camera settings");
            return false;
        }
    }
    
    if (StagedViewportCamera != nullptr)
    {
        ViewportCamera.SetTransform(StagedViewportCamera->GetTransform());

        if (!CopyCameraSettings(*StagedViewportCamera, ViewportCamera))
        {
            UE_LOG(Error, "Scene Load failed: Cannot commit viewport camera settings");
            return false;
        }
    }

    if (Context.TransformGizmo != nullptr)
    {
        Context.TransformGizmo->CancelDrag();
    }

    UWorld* OldWorld = Context.World;

    Context.World = StagedWorld.release();
    Context.ClearSelection();

    delete OldWorld;

    FEngineStatics::NextUUID = (std::max)(FEngineStatics::NextUUID, SavedNextUUID);

    UUIDRollback.bCommitted = true;

    UE_LOG(Info, "Scene loaded: %s", FilePath.string().c_str());
    return true;
}