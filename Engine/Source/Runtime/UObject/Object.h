
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

    // Destroyed but not yet deleted (Unreal's "garbage" / pending kill state).
    // The owner deletes it later, at a safe point in the frame.
    void MarkAsGarbage() { bGarbage = true; }
    bool IsGarbage() const { return bGarbage; }

    // Unique for the whole run and never reused (unlike UUID, which can be reassigned).
    // Used by weak object pointers to tell a live object from a recycled address.
    uint32 GetSerialNumber() const { return SerialNumber; }

    // True while an object with this address and serial number is alive.
    static bool IsAlive(const UObject* Object, uint32 SerialNumber);

private:
    uint32 UUID = 0;
    uint32 SerialNumber = 0;
    bool bGarbage = false;
    uint32 InternalIndex = 0;

    FName ObjectName;
    UObject* Outer = nullptr;
};

extern TArray<UObject*> GUObjectArray;

// Unreal-style validity check: not null and not destroyed.
inline bool IsValid(const UObject* Object)
{
    return Object != nullptr && !Object->IsGarbage();
}
