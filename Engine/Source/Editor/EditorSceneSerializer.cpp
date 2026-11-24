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

extern TArray<UObject*> GUObjectArray;

namespace
{
    constexpr std::uintmax_t MaxSceneFileBytes = 64ull * 1024ull * 1024ull;
    
    enum class ESceneActorType
    {
        StaticMeshActor,
        CameraActor
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

    bool ReadObjectMap(const FJsonObject& ObjectMap, bool bIsCamera, FAssetManager& AssetManager, URenderer& Renderer, TArray<FSceneObjectRecord>& OutRecords, TSet<uint32>& SeenUUIDs, uint32& OutMaxUUID)
    {
        for (const auto& Pair : ObjectMap.GetValues())
        {
            FSceneObjectRecord Record;
            Record.bIsCamera = bIsCamera;

            if (!TryParseUUID(Pair.first, Record.UUID) || !SeenUUIDs.Add(Record.UUID))
            {
                return false;
            }

            const FJsonObject* ObjectData = nullptr;

            if (!TryGetJsonObject(Pair.second, ObjectData) || !ReadTransform(*ObjectData, Record.Transform))
            {
                return false;
            }

            if (bIsCamera && ObjectData->HasField("Settings"))
            {
                const TSharedPtr<FJsonValue> SettingsValue = ObjectData->GetField("Settings");
                const FJsonObject* SettingsObject = nullptr;

                if (!TryGetJsonObject(SettingsValue, SettingsObject))
                {
                    return false;
                }

                Record.CameraSettings = SettingsValue->AsObject();
            }

            if (!bIsCamera)
            {
                FString AssetPath;

                if (!ObjectData->TryGetStringField("AssetPath", AssetPath))
                {
                    return false;
                }

                Record.StaticMesh = OBJImporter::LoadOBJAsset(AssetPath, AssetManager, Renderer);

                if (Record.StaticMesh == nullptr)
                {
                    return false;
                }

                if (ObjectData->HasField("TexturePath"))
                {
                    FString TexturePath;

                    if (!ObjectData->TryGetStringField("TexturePath", TexturePath) || TexturePath.IsEmpty())
                    {
                        return false;
                    }

                    Record.Material = TextureImporter::LoadTextureMaterial(TexturePath, AssetManager, Renderer);

                    if (Record.Material == nullptr)
                    {
                        return false;
                    }
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
    TSharedPtr<FJsonObject> Primitives = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> Cameras = MakeShared<FJsonObject>();

    Root->SetNumberField("Version", 1);
    Root->SetNumberField("NextUUID", FEngineStatics::NextUUID);

    ULevel* Level = Context.World != nullptr ? Context.World->GetPersistentLevel() : nullptr;

    if (Level == nullptr)
    {
        return false;
    }

    TSharedPtr<FJsonObject> Actors = MakeShared<FJsonObject>();

    Root->SetNumberField("Version", 2);
    Root->SetNumberField("NextUUID", FEngineStatics::NextUUID);

    for (AActor* Actor : Level->GetActors())
    {
        if (Actor == nullptr)
        {
            continue;
        }

        FJsonObject ActorData;
        USceneComponent* RootComponent = Actor->GetRootComponent();

        if (RootComponent == nullptr)
        {
            UE_LOG(Error, "Scene Save failed: Actor has no root component");
            return false;
        }

        WriteTransform(ActorData, Actor->GetActorTransform());
        ActorData.SetStringField("Name", Actor->GetName());
        ActorData.SetNumberField("RootComponentUUID", RootComponent->GetUUID());

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

            UMaterial* Material = Component->GetMaterial();

            if (Material != nullptr && Material->GetBaseColorTexture() != nullptr)
            {
                const FString& TexturePath = Material->GetBaseColorTexture()->GetAssetPath();

                if (TexturePath.IsEmpty())
                {
                    return false;
                }

                ActorData.SetStringField("TexturePath", TexturePath);
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
        else
        {
            UE_LOG(Error, "Scene Save failed: Unsupported Actor class");
            return false;
        }

        Actors->SetObjectField(std::to_string(Actor->GetUUID()).c_str(), std::move(ActorData));
    }

    Root->SetObjectField("Actors", std::move(*Actors));

    Root->SetObjectField("Primitives", std::move(*Primitives));
    Root->SetObjectField("Cameras", std::move(*Cameras));

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

    if (!Root->TryGetNumberField("Version", Version) || Version != 1.0 || !Root->TryGetNumberField("NextUUID", NextUUIDNumber))
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

    TArray<FSceneObjectRecord> Records;
    TSet<uint32> SeenUUIDs;
    uint32 MaxUUID = 0;

    if (!ReadObjectMap(*PrimitiveObject, false, AssetManager, Renderer, Records, SeenUUIDs, MaxUUID))
    {
        UE_LOG(Error, "Scene Load failed: Invalid primitive data");
        return false;
    }

    if (CameraObject != nullptr && !ReadObjectMap(*CameraObject, true, AssetManager, Renderer, Records, SeenUUIDs, MaxUUID))
    {
        UE_LOG(Error, "Scene Load failed: Invalid camera data");
        return false;
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

    for (const FSceneObjectRecord& Record : Records)
    {
        if (HasUUIDCollision(Record.UUID, Context))
        {
            UE_LOG(Error, "Scene Load failed: UUID collision (%u)", Record.UUID);
            return false;
        }
    }

    if (FEngineStatics::NextUUID == (std::numeric_limits<uint32>::max)() && !Records.IsEmpty())
    {
        UE_LOG(Error, "Scene Load failed: Runtime UUID range exhausted");
        return false;
    }

    FScopedNextUUIDRollback UUIDRollback;

    FEngineStatics::NextUUID = (std::max)(FEngineStatics::NextUUID, SavedNextUUID);

    std::unique_ptr<UWorld> StagedWorld(NewObject<UWorld>(nullptr, FName("StagedSceneWorld")));

    if (StagedWorld == nullptr || StagedWorld->GetPersistentLevel() == nullptr)
    {
        return false;
    }

    auto DestroyStagedObjects = [&StagedObjects]()
    {
        for (UObject* Object : StagedObjects)
        {
            delete Object;
        }

        StagedObjects.Reset();
    };

    for (const FSceneObjectRecord& Record : Records)
    {
        UObject* NewObjectInstance = nullptr;

        if (Record.bIsCamera)
        {
            UCameraComponent* Camera = NewObject<UCameraComponent>();

            if (Camera != nullptr)
            {
                Camera->SetTransform(Record.Transform);

                if (!ApplyCameraSettings(*Camera, Record.CameraSettings.Get()))
                {
                    delete Camera;
                    DestroyStagedObjects();
                    UE_LOG(Error, "Scene Load failed: Invalid camera settings");
                    return false;
                }
            }

            NewObjectInstance = Camera;
        }
        else
        {
            UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>();

            if (Component != nullptr)
            {
                Component->SetStaticMesh(Record.StaticMesh);
                Component->SetMaterial(Record.Material);
                Component->SetTransform(Record.Transform);
            }

            NewObjectInstance = Component;
        }

        if (NewObjectInstance == nullptr)
        {
            DestroyStagedObjects();
            UE_LOG(Error, "Scene Load failed: Object construction failed");
            return false;
        }

        StagedObjects.Add(NewObjectInstance);
    }
    
    std::unique_ptr<UCameraComponent> StagedViewportCamera;

    if (bHasViewportCameraData)
    {
        StagedViewportCamera.reset(NewObject<UCameraComponent>());

        if (StagedViewportCamera == nullptr)
        {
            DestroyStagedObjects();
            UE_LOG(Error, "Scene Load failed: Cannot create staged viewport camera");
            return false;
        }

        if (!CopyCameraSettings(ViewportCamera, *StagedViewportCamera))
        {
            DestroyStagedObjects();
            UE_LOG(Error, "Scene Load failed: Cannot copy viewport camera settings");
            return false;
        }

        StagedViewportCamera->SetTransform(ViewportCameraTransform);

        if (!ApplyCameraSettings(*StagedViewportCamera, ViewportCameraSettings.Get()))
        {
            DestroyStagedObjects();
            UE_LOG(Error, "Scene Load failed: Cannot apply viewport camera settings");
            return false;
        }
    }
    
    if (StagedViewportCamera != nullptr)
    {
        ViewportCamera.SetTransform(StagedViewportCamera->GetTransform());

        if (!CopyCameraSettings(*StagedViewportCamera, ViewportCamera))
        {
            DestroyStagedObjects();
            UE_LOG(Error, "Scene Load failed: Cannot commit viewport camera settings");
            return false;
        }
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