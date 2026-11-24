
#pragma once

#include "Core/STL.h"
#include "Core/Name.h"

class UClass;

class UObject
{
public:
    explicit UObject(UObject* InOuter = nullptr, FName InName = FName());
    virtual ~UObject();

    uint32 GetUUID() const;
    void SetUUID(uint32 uuid);
    uint32 GetInternalIndex() const;

    const FName& GetFName() const { return ObjectName; }
    FString GetName() const;
    FString GetPathName() const;
    void SetName(const FName& InName) { ObjectName = InName; }

    UObject* GetOuter() const { return Outer; }
    bool SetOuter(UObject* InOuter);

    template<typename T>
    T* GetTypedOuter() const
    {
        for (UObject* Current = Outer; Current != nullptr; Current = Current->GetOuter())
        {
            if (Current->IsA<T>())
            {
                return static_cast<T*>(Current);
            }
        }

        return nullptr;
    }

    static UClass* StaticClass();
    virtual UClass* GetClass() const;
    bool IsA(const UClass* InClass) const;

    void ProcessEvent(const char* FunctionName, void* Parameters);

    template<typename T>
    bool IsA() const { return IsA(T::StaticClass()); }

private:
    uint32 UUID = 0;
    uint32 InternalIndex = 0;

    FName ObjectName;
    UObject* Outer = nullptr;
};

extern TArray<UObject*> GUObjectArray;
