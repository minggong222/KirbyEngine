#include "Object.h"

#include "Class.h"
#include "Core/EngineStatics.h"

TArray<UObject*> GUObjectArray;

UObject::UObject()
{
    UUID = FEngineStatics::GenUUID();
    InternalIndex = static_cast<uint32>(GUObjectArray.size());
    GUObjectArray.Add(this);
}

UObject::~UObject()
{
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
    static UClass* ClassInfo = new UClass("UObject", nullptr, sizeof(UObject));
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

void* UObject::operator new(SIZE_T size)
{
    void* Ptr = malloc(size);
    if (!Ptr) throw std::bad_alloc();
        
    FEngineStatics::TotalAllocationBytes += static_cast<uint32>(size);
    FEngineStatics::TotalAllocationCount += 1;
    return Ptr;
}

void UObject::operator delete(void* ptr, SIZE_T size)
{
    FEngineStatics::TotalAllocationBytes -= static_cast<uint32>(size);
    FEngineStatics::TotalAllocationCount -= 1;
    free(ptr);
}
