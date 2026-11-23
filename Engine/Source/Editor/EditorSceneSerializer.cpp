#include "EditorSceneSerializer.h"

#include "EditorObjectContext.h"

#include "Core/EngineStatics.h"
#include "Core/Logging/Log.h"

#include "Components/CameraComponent.h"
#include "Components/StaticMeshComponent.h"

#include "Resources/AssetManager.h"
#include "Resources/PrimitiveMeshAssets.h"
#include "Resources/StaticMesh.h"

#include "UObject/Object.h"

#include "Json/Dom/JsonObject.h"
#include "Json/Serialization/JsonSerializer.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <unordered_set>

extern TArray<UObject*> GUObjectArray;

namespace
{
    struct FSceneObjectRecord
    {
        uint32 UUID = 0;
        FTransform Transform;
        UStaticMesh* StaticMesh = nullptr;
        bool bIsCamera = false;
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
            if (ExistingObject == nullptr || Context.Objects.Contains(ExistingObject))
            {
                continue;
            }

            if (ExistingObject->GetUUID() == UUID)
            {
                return true;
            }
        }

        return false;
    }

    bool ReadObjectMap(const FJsonObject& ObjectMap, bool bIsCamera, FAssetManager& AssetManager, TArray<FSceneObjectRecord>& OutRecords, TSet<uint32>& SeenUUIDs, uint32& OutMaxUUID)
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

            if (!bIsCamera)
            {
                FString Type;

                if (!ObjectData->TryGetStringField("Type", Type))
                {
                    return false;
                }

                if (Type == "Cube")
                {
                    Record.StaticMesh = AssetManager.Find<UStaticMesh>(BuiltinAssetPaths::Cube);
                }
                else if (Type == "Sphere")
                {
                    Record.StaticMesh = AssetManager.Find<UStaticMesh>(BuiltinAssetPaths::Sphere);
                }
                else
                {
                    return false;
                }

                if (Record.StaticMesh == nullptr)
                {
                    return false;
                }
            }

            OutMaxUUID = max(OutMaxUUID, Record.UUID);
            OutRecords.Add(std::move(Record));
        }

        return true;
    }
}

bool FSceneSerializer::Save(const std::filesystem::path& FilePath, const FEditorObjectContext& Context)
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

    for (UObject* Object : Context.Objects)
    {
        if (Object == nullptr)
        {
            continue;
        }

        if (UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Object))
        {
            UStaticMesh* Mesh = Component->GetStaticMesh();

            if (Mesh == nullptr)
            {
                UE_LOG(Error, "Scene Save failed: StaticMeshComponent has no mesh");
                return false;
            }

            const std::string& AssetPath = Mesh->GetAssetPath().ToStd();
            FString Type;

            if (AssetPath == BuiltinAssetPaths::Cube)
            {
                Type = "Cube";
            }
            else if (AssetPath == BuiltinAssetPaths::Sphere)
            {
                Type = "Sphere";
            }
            else
            {
                UE_LOG(Error, "Scene Save failed: Unsupported StaticMesh");
                return false;
            }

            FJsonObject PrimitiveData;
            WriteTransform(PrimitiveData, Component->GetTransform());
            PrimitiveData.SetStringField("Type", Type);

            Primitives->SetObjectField(std::to_string(Object->GetUUID()).c_str(), std::move(PrimitiveData));
        }
        else if (UCameraComponent* Camera = Cast<UCameraComponent>(Object))
        {
            FJsonObject CameraData;
            WriteTransform(CameraData, Camera->GetTransform());

            Cameras->SetObjectField(std::to_string(Object->GetUUID()).c_str(), std::move(CameraData));
        }
        else
        {
            UE_LOG(Error, "Scene Save failed: Unsupported UObject class");
            return false;
        }
    }

    Root->SetObjectField("Primitives", std::move(*Primitives));
    Root->SetObjectField("Cameras", std::move(*Cameras));

    FString JsonString;
    FString SerializeError;

    if (!FJsonSerializer::Serialize(Root, JsonString, true, &SerializeError))
    {
        UE_LOG(Error, "Scene Save failed: %s", SerializeError.c_str());
        return false;
    }

    std::ofstream OutputFile(FilePath, std::ios::binary | std::ios::out | std::ios::trunc);

    if (!OutputFile.is_open())
    {
        UE_LOG(Error, "Scene Save failed: Cannot open output file");
        return false;
    }

    const std::string& JsonText = JsonString.ToStd();

    OutputFile.write(JsonText.data(), static_cast<std::streamsize>(JsonText.size()));
    OutputFile.flush();

    if (!OutputFile.good())
    {
        UE_LOG(Error, "Scene Save failed: File write error");
        return false;
    }

    UE_LOG(Info, "Scene saved: %s", FilePath.string().c_str());
    return true;
}

bool FSceneSerializer::Load(const std::filesystem::path& FilePath, FEditorObjectContext& Context, FAssetManager& AssetManager)
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

    std::string JsonText((std::istreambuf_iterator<char>(InputFile)), std::istreambuf_iterator<char>());

    if (InputFile.bad())
    {
        UE_LOG(Error, "Scene Load failed: File read error");
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

    UStaticMeshComponent::StaticClass();
    UCameraComponent::StaticClass();

    TArray<FSceneObjectRecord> Records;
    TSet<uint32> SeenUUIDs;
    uint32 MaxUUID = 0;

    if (!ReadObjectMap(*PrimitiveObject, false, AssetManager, Records, SeenUUIDs, MaxUUID))
    {
        UE_LOG(Error, "Scene Load failed: Invalid primitive data");
        return false;
    }

    if (CameraObject != nullptr && !ReadObjectMap(*CameraObject, true, AssetManager, Records, SeenUUIDs, MaxUUID))
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

    FEngineStatics::NextUUID = max(FEngineStatics::NextUUID, SavedNextUUID);

    TArray<UObject*> StagedObjects;

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
                Camera->SetUUID(Record.UUID);
            }

            NewObjectInstance = Camera;
        }
        else
        {
            UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>();

            if (Component != nullptr)
            {
                Component->SetStaticMesh(Record.StaticMesh);
                Component->SetTransform(Record.Transform);
                Component->SetUUID(Record.UUID);
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

    for (UObject* ExistingObject : Context.Objects)
    {
        delete ExistingObject;
    }

    Context.Objects.Reset();

    for (UObject* StagedObject : StagedObjects)
    {
        Context.Objects.Add(StagedObject);
    }

    StagedObjects.Reset();
    Context.SelectedObject = nullptr;

    FEngineStatics::NextUUID = max(FEngineStatics::NextUUID, SavedNextUUID);

    UE_LOG(Info, "Scene loaded: %s", FilePath.string().c_str());
    return true;
}