#pragma once
#include "STL.h"

class FEngineStatics
{
public:
    static uint32 GenUUID()
    {
        return NextUUID++;
    }
    inline static uint32 NextUUID = 1;
    
    inline static uint32 TotalAllocationBytes;
    inline static uint32 TotalAllocationCount;
};
