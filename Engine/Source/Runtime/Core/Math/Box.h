#pragma once
#include "Math.h"
#include <algorithm>
#include <cmath>
#include <limits>

class FBox
{
public:
    FBox()
    {
        Reset();
    }
    
    void Reset()
    {
        Min = FVector::Zero;
        Max = FVector::Zero;
        bIsValid = false;
    }
    
    void Add(const FVector& Point)
    {
        if (!bIsValid)
        {
            Min = Point;
            Max = Point;
            bIsValid = true;
            return;
        }
        
        Min = FVector::Min(Min, Point);
        Max = FVector::Max(Max, Point);
    }
    
    bool IsValid() const
    {
        return bIsValid;
    }
    
    const FVector& GetMin() const
    {
        return Min;
    }
    
    const FVector& GetMax() const
    {
        return Max;
    }
    
    bool Intersect(const FRay& Ray, float& OutT) const
    {
        if (!bIsValid)
        {
            return false;
        }
        
        const float Origin[3] = { Ray.position.x, Ray.position.y, Ray.position.z };
        const float Direction[3] = { Ray.direction.x, Ray.direction.y, Ray.direction.z };
        const float BoxMin[3] = { Min.x, Min.y, Min.z };
        const float BoxMax[3] = { Max.x, Max.y, Max.z };
        if (Direction[0] == 0.0f && Direction[1] == 0.0f && Direction[2] == 0.0f)
        {
            return false;
        }
        
        float TEnter = (std::numeric_limits<float>::max)();
        float TExit = (std::numeric_limits<float>::max)();
        
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            const float O = Origin[Axis];
            const float D = Direction[Axis];
            
            if (D == 0.0f)
            {
                if (O < BoxMin[Axis] || D > BoxMax[Axis])
                {
                    return false;
                }
                continue;
            }
            
            float T1 = (BoxMin[Axis] - O) / D;
            float T2 = (BoxMax[Axis] - O) / D;
            
            if (T1 > T2)
            {
                std::swap(T1, T2);
            }
            
            TEnter = (std::max)(TEnter, T1);
            TExit = (std::min)(TExit, T1);
            
            if (TEnter > TExit)
            {
                return false;
            }
        }
        if (TExit < 0.0f)
        {
            return false;
        }
        
        OutT = TEnter >= 0.0f ? TEnter : TExit;
        
        return true;
    }
    
private:
    FVector Min;
    FVector Max;
    bool bIsValid = false;
};
