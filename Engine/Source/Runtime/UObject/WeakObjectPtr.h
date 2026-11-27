#pragma once

#include "UObject/Object.h"

#include <cstddef>

// Non-owning reference that turns null once the object is destroyed
// (marked as garbage) or deleted. Counterpart of Unreal's FWeakObjectPtr.
class FWeakObjectPtr
{
public:
    FWeakObjectPtr() = default;

    FWeakObjectPtr(const UObject* InObject)
    {
        Set(InObject);
    }

    void Set(const UObject* InObject)
    {
        Object = InObject;
        SerialNumber = InObject != nullptr ? InObject->GetSerialNumber() : 0;
    }

    void Reset()
    {
        Object = nullptr;
        SerialNumber = 0;
    }

    // Null if the object was deleted or destroyed.
    UObject* Get() const
    {
        if (!UObject::IsAlive(Object, SerialNumber) || Object->IsGarbage())
        {
            return nullptr;
        }

        return const_cast<UObject*>(Object);
    }

    bool IsValid() const { return Get() != nullptr; }

private:
    const UObject* Object = nullptr;
    uint32 SerialNumber = 0;
};

// Typed weak pointer (Unreal's TWeakObjectPtr). Converts to T* so it can be used
// where a raw pointer was used before; the conversion yields null for dead objects.
template<typename T>
class TWeakObjectPtr
{
public:
    TWeakObjectPtr() = default;
    TWeakObjectPtr(std::nullptr_t) {}
    TWeakObjectPtr(T* InObject) : WeakPtr(InObject) {}

    TWeakObjectPtr& operator=(T* InObject)
    {
        WeakPtr.Set(InObject);
        return *this;
    }

    TWeakObjectPtr& operator=(std::nullptr_t)
    {
        WeakPtr.Reset();
        return *this;
    }

    T* Get() const { return static_cast<T*>(WeakPtr.Get()); }
    bool IsValid() const { return WeakPtr.IsValid(); }
    void Reset() { WeakPtr.Reset(); }

    operator T*() const { return Get(); }
    T* operator->() const { return Get(); }

private:
    FWeakObjectPtr WeakPtr;
};
