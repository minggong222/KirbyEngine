#pragma once
#include "SceneComponent.h"
#include "Core/Math/Box.h"

class UPrimitiveComponent : public USceneComponent
{
    DECLARE_CLASS(UPrimitiveComponent, USceneComponent)
public:
    const FBox& GetLocalBounds() const { return LocalBounds; }
    void SetLocalBounds(const FBox& InBounds) { LocalBounds = InBounds; }
    
private:
    FBox LocalBounds;
};
