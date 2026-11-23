
#pragma once
#include "Core/Math/Transform.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.generated.h"

UCLASS()
class USceneComponent : public UObject
{
    GENERATED_BODY()
public:
    USceneComponent() = default;
    ~USceneComponent();
    
    FTransform GetTransform() const { return Transform; }
    void SetTransform(FTransform InTransform){ Transform = InTransform; }
    
    FTransform GetRelativeTransform() const { return Transform; }
    void SetRelativeTransform(FTransform InTransform){ Transform = InTransform; }
    
    FMatrix GetWorldMatrix() const;
    FTransform GetWorldTransform() const;
    bool SetWorldTransform(const FTransform& InWorldTransform);
    
    bool AttachToComponent(USceneComponent* InParent, bool bKeepWorldTransform = true);
    void DetachFromComponent(bool bKeepWorldTransform = true);
    
    USceneComponent* GetAttachParent() const { return Parent; }
    const TArray<USceneComponent*>& GetAttachChildren() const { return Children; }
private:
    UPROPERTY(EditAnywhere, Category="Transform")
    FTransform Transform;

    USceneComponent* Parent = nullptr;
    TArray<USceneComponent*> Children;
};
