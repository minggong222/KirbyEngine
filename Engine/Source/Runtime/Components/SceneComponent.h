#pragma once
#include "Core/Math.h"
#include "UObject/Object.h"

class USceneComponent : public UObject
{
public:
    FVector GetLocation() const { return RelativeLocation; }
    void SetLocation(FVector newLocation){ RelativeLocation = newLocation; }
    FVector GetRotation() const { return RelativeRotation; }
    void SetRotation(FVector newRotation){ RelativeRotation = newRotation; }
    FVector GetScale3D() const { return RelativeScale3D; }
    void SetScale3D(FVector newScale3D){ RelativeScale3D = newScale3D; }    
private:
    FVector RelativeLocation = { 0.0f, 0.0f, 0.0f };
    FVector RelativeRotation = { 0.0f, 0.0f, 0.0f };
    FVector RelativeScale3D = { 0.0f, 0.0f, 0.0f };
};
