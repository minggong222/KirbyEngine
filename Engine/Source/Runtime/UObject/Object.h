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

    void ProcessEvent(const char* FunctionName, void* Parameters);
    
    template<typename T>
    bool IsA() const { return IsA(T::StaticClass()); }
private:
    uint32 UUID;
    uint32 InternalIndex;
};


extern TArray<UObject*> GUObjectArray;