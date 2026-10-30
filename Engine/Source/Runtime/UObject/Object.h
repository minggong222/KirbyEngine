#pragma once
#include "Core/STL.h"

class UClass;

class UObject
{
public:
    UObject();
    virtual ~UObject();
    
    uint32 GetUUID() const;
    void SetUUID(uint32 uuid);
    uint32 GetInternalIndex() const;
    
    static UClass* StaticClass();
    virtual UClass* GetClass() const;
    bool IsA(const UClass* InClass) const;
    
    template<typename T>
    bool IsA() const { return IsA(T::StaticClass()); }
    
    void* operator new(SIZE_T size);
    void operator delete(void* ptr, SIZE_T size);
private:
    uint32 UUID;
    uint32 InternalIndex;
};
