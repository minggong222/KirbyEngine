#include "Object.h"

#include "Class.h"
#include "Core/EngineStatics.h"

#include <unordered_map>

TArray<UObject*> GUObjectArray;

namespace
{
    // Live object address -> serial number, for weak pointer validation.
    std::unordered_map<const UObject*, uint32>& GetLiveObjectSerials()
    {
        static std::unordered_map<const UObject*, uint32> LiveObjectSerials;
        return LiveObjectSerials;
    }

    uint32 GNextObjectSerialNumber = 1;
}

UObject::UObject(UObject* InOuter, FName InName)
    : ObjectName(InName)
{
    if (InOuter != nullptr)
    {
        const bool bOuterSet = SetOuter(InOuter);
        assert(bOuterSet);
    }

    UUID = FEngineStatics::GenUUID();
    SerialNumber = GNextObjectSerialNumber++;
    GetLiveObjectSerials()[this] = SerialNumber;

    InternalIndex = static_cast<uint32>(GUObjectArray.size());
    GUObjectArray.Add(this);
}

bool UObject::IsAlive(const UObject* Object, uint32 InSerialNumber)
{
    if (Object == nullptr || InSerialNumber == 0)
    {
        return false;
    }

    const auto& LiveObjectSerials = GetLiveObjectSerials();
    const auto Found = LiveObjectSerials.find(Object);
    return Found != LiveObjectSerials.end() && Found->second == InSerialNumber;
}

UObject::~UObject()
{
    GetLiveObjectSerials().erase(this);

    const int32 Index = static_cast<uint32>(InternalIndex);
    if (!GUObjectArray.IsValidIndex(Index))
    {
        assert(false && "Invalid UObject InternalIndex");
        return;
    }
    UObject* MovedObject = nullptr;
    const int32 LastIndex = GUObjectArray.Num() - 1;
    
    if (Index != LastIndex)
    {
        MovedObject = GUObjectArray[LastIndex];
    }
    GUObjectArray.RemoveAtSwap(Index);
    
    if (MovedObject != nullptr)
    {
        MovedObject->InternalIndex = static_cast<uint32>(Index);
    }
}

uint32 UObject::GetUUID() const
{
    return UUID;
}

void UObject::SetUUID(uint32 uuid)
{
    UUID = uuid;
}

uint32 UObject::GetInternalIndex() const
{
    return InternalIndex;
}

UClass* UObject::StaticClass()
{
    static UClass* ClassInfo = new UClass("UObject", nullptr, sizeof(UObject), []() -> UObject* { return new UObject(); });
    return ClassInfo;
}

UClass* UObject::GetClass() const
{
    return StaticClass();
}

bool UObject::IsA(const UClass* InClass) const
{
    if (InClass == nullptr)
    {
        return false;
    }
    const UClass* ActualClass = GetClass();
    
    return ActualClass != nullptr && ActualClass->IsChildOf(InClass);
}

void UObject::ProcessEvent(const char* FunctionName, void* Parameters)
{
    if (FunctionName == nullptr)
    {
        assert(false && "FunctionName is null");
        return;
    }

    UClass* ObjectClass = GetClass();

    if (ObjectClass == nullptr)
    {
        assert(false && "Object class is null");
        return;
    }

    const UFunction* Function = ObjectClass->FindFunction(FunctionName);

    if (Function == nullptr)
    {
        assert(false && "Reflected function not found");
        return;
    }

    if (!Function->Invoke(this, Parameters))
    {
        assert(false && "Failed to invoke reflected function");
    }
}

FString UObject::GetName() const
{
    if (!ObjectName.IsNone())
    {
        return ObjectName.ToString();
    }

    const UClass* ObjectClass = GetClass();

    return ObjectClass != nullptr ? ObjectClass->GetName() : "UObject";
}

FString UObject::GetPathName() const
{
    TArray<FString> Names;

    for (const UObject* Current = this; Current != nullptr; Current = Current->GetOuter())
    {
        Names.Add(Current->GetName());
    }

    FString Result;

    for (int32 Index = Names.Num() - 1; Index >= 0; --Index)
    {
        if (!Result.empty())
        {
            Result.Append(".");
        }

        Result.Append(Names[Index].ToStd());
    }

    return Result;
}

bool UObject::SetOuter(UObject* InOuter)
{
    if (Outer == InOuter)
    {
        return true;
    }

    // 이미 소유자가 설정된 객체의 소유자를 임의로 바꾸지 않는다.
    if (Outer != nullptr)
    {
        return false;
    }

    if (InOuter == this)
    {
        return false;
    }

    // Outer 순환 참조 방지
    for (UObject* Current = InOuter; Current != nullptr; Current = Current->GetOuter())
    {
        if (Current == this)
        {
            return false;
        }
    }

    Outer = InOuter;

    return true;
}