#include "EngineStatics.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>

namespace
{
    struct FTrackedAllocationHeader
    {
        void* RawMemory;
        std::size_t Size;
    };
    
    void* AllocateTracked(std::size_t Size, std::size_t Alignment)
    {
        Alignment = (std::max)(Alignment, alignof(FTrackedAllocationHeader));
        
        const std::size_t ActualSize = Size == 0 ? 1 : Size;
        const std::size_t MaximumSize = (std::numeric_limits<std::size_t>::max)();
        
        if (ActualSize > MaximumSize - sizeof(FTrackedAllocationHeader) - (Alignment - 1))
        {
            throw std::bad_alloc();
        }
        
        const std::size_t TotalSize = ActualSize + sizeof(FTrackedAllocationHeader) + Alignment - 1;
        void* RawMemory = std::malloc(TotalSize);
        
        if (RawMemory == nullptr)
        {
            throw std::bad_alloc();
        }
        
        const std::uintptr_t StartAddress = reinterpret_cast<std::uintptr_t>(RawMemory) + sizeof(FTrackedAllocationHeader);
        const std::uintptr_t AlignedAddress = (StartAddress + Alignment - 1) & ~(static_cast<std::uintptr_t>(Alignment) - 1);
        
        auto* Header = reinterpret_cast<FTrackedAllocationHeader*>(AlignedAddress - sizeof(FTrackedAllocationHeader));
        ::new (Header) FTrackedAllocationHeader{RawMemory, Size};
        
        FEngineStatics::AddAllocation(Size);
        
        return reinterpret_cast<void*>(AlignedAddress);
    }
    
    void FreeTracked(void* Memory) noexcept
    {
        if (Memory == nullptr)
        {
            return;
        }
        
        const std::uintptr_t Address = reinterpret_cast<std::uintptr_t>(Memory);
        auto* Header = reinterpret_cast<FTrackedAllocationHeader*>(Address - sizeof(FTrackedAllocationHeader));
        
        void* RawMemory = Header->RawMemory;
        const std::size_t Size = Header->Size;
        
        Header->~FTrackedAllocationHeader();
        
        FEngineStatics::RemoveAllocation(Size);
        std::free(RawMemory);
    }
}

void* operator new(std::size_t Size)
{
    return AllocateTracked(Size, alignof(std::max_align_t));
}

void* operator new[](std::size_t Size)
{
    return AllocateTracked(Size, alignof(std::max_align_t));
}

void operator delete(void* Memory) noexcept
{
    FreeTracked(Memory);
}

void operator delete[](void* Memory) noexcept
{
    FreeTracked(Memory);
}

void operator delete(void* Memory, std::size_t Size) noexcept
{
    FreeTracked(Memory);
}

void operator delete[](void* Memory, std::size_t Size) noexcept
{
    FreeTracked(Memory);
}

void* operator new(std::size_t Size, std::align_val_t Alignment)
{
    return AllocateTracked(Size, static_cast<std::size_t>(Alignment));
}

void* operator new[](std::size_t Size, std::align_val_t Alignment)
{
    return AllocateTracked(Size, static_cast<std::size_t>(Alignment));
}

void operator delete(void* Memory, std::align_val_t) noexcept
{
    FreeTracked(Memory);
}

void operator delete[](void* Memory, std::align_val_t) noexcept
{
    FreeTracked(Memory);
}

void operator delete(void* Memory, std::size_t, std::align_val_t) noexcept
{
    FreeTracked(Memory);
}

void operator delete[](void* Memory, std::size_t, std::align_val_t) noexcept
{
    FreeTracked(Memory);
}

void* operator new(std::size_t Size, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new(Size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t Size, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new[](Size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(void* Memory, const std::nothrow_t&) noexcept
{
    FreeTracked(Memory);
}

void operator delete[](void* Memory, const std::nothrow_t&) noexcept
{
    FreeTracked(Memory);
}

void* operator new(std::size_t Size, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new(Size, Alignment);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t Size, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new[](Size, Alignment);
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete(void* Memory, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
    FreeTracked(Memory);
}

void operator delete[](void* Memory, std::align_val_t Alignment, const std::nothrow_t&) noexcept
{
    FreeTracked(Memory);
}