#pragma once

#include "STL.h"
#include <atomic>

class FEngineStatics
{
public:
    static uint32 GenUUID()
    {
        return NextUUID++;
    }

    static void AddAllocation(SIZE_T Size) noexcept
    {
        TotalAllocationBytes.fetch_add(static_cast<uint64>(Size), std::memory_order_relaxed);
        TotalAllocationCount.fetch_add(1, std::memory_order_relaxed);
    }

    static void RemoveAllocation(SIZE_T Size) noexcept
    {
        TotalAllocationBytes.fetch_sub(static_cast<uint64>(Size), std::memory_order_relaxed);
        TotalAllocationCount.fetch_sub(1, std::memory_order_relaxed);
    }

    static uint64 GetTotalAllocationBytes() noexcept
    {
        return TotalAllocationBytes.load(std::memory_order_relaxed);
    }

    static uint64 GetTotalAllocationCount() noexcept
    {
        return TotalAllocationCount.load(std::memory_order_relaxed);
    }

    inline static uint32 NextUUID = 1;

private:
    inline static std::atomic<uint64> TotalAllocationBytes{0};
    inline static std::atomic<uint64> TotalAllocationCount{0};
};