#include "Object.h"

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
    GUObjectArray.RemoveAtSwap(InternalIndex);
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
