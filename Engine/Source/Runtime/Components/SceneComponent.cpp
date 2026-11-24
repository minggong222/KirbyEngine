#include "SceneComponent.h"
#include <cmath>

namespace
{
    bool TryMakeTransform(const FMatrix& Matrix, FTransform& OutTransform)
    {
        FVector Scale;
        FVector Translation;
        FQuat Rotation;
        
        if (!Matrix.Decompose(Scale, Rotation, Translation))
        {
            return false;
        }
        
        Rotation.Normalize();
        OutTransform = FTransform(Translation, FRotator(Rotation), Scale);
        
        return true;
    }
}


USceneComponent::~USceneComponent()
{
    DetachFromComponent(true);
    
    while (!Children.IsEmpty())
    {
        USceneComponent* Child = Children.Last();
        
        if (Child == nullptr)
        {
            Children.Pop();
            continue;
        }
        
        Child->DetachFromComponent(true);
    }
}

FMatrix USceneComponent::GetWorldMatrix() const
{
    const FMatrix LocalMatrix = Transform.GetLocalMatrix();
    
    if (Parent == nullptr)
    {
        return LocalMatrix;
    }
    
    return LocalMatrix * Parent->GetWorldMatrix();
}

FTransform USceneComponent::GetWorldTransform() const
{
    FTransform WorldTransform;
    
    if (!TryMakeTransform(GetWorldMatrix(), WorldTransform))
    {
        return Transform;
    }
    
    return WorldTransform;
}

bool USceneComponent::SetWorldTransform(const FTransform& InWorldTransform)
{
    FMatrix RelativeMatrix = InWorldTransform.GetLocalMatrix();
    
    if (Parent != nullptr)
    {
        const FMatrix ParentWorldMatrix = Parent->GetWorldMatrix();
        const float Determinant = ParentWorldMatrix.Determinant();
        
        if (!std::isfinite(Determinant) || std::fabs(Determinant) <= 1.0e-8f)
        {
            return false;
        }
        RelativeMatrix = RelativeMatrix * ParentWorldMatrix.Invert();
    }
    
    FTransform NewRelativeTransform;
    
    if (!TryMakeTransform(RelativeMatrix, NewRelativeTransform))
    {
        return false;
    }
    
    Transform = NewRelativeTransform;
    return true;
}

bool USceneComponent::AttachToComponent(USceneComponent* InParent, bool bKeepWorldTransform)
{
    if (InParent == this)
    {
        return false;
    }

    if (InParent != nullptr && (GetOwner() == nullptr || InParent->GetOwner() != GetOwner()))
    {
        return false;
    }

    if (Parent == InParent)
    {
        return true;
    }
    
    for (USceneComponent* Ancestor = InParent; Ancestor != nullptr; Ancestor = Ancestor->Parent)
    {
        if (Ancestor == this)
        {
            return false;
        }
    }
    
    FTransform NewRelativeTransform = Transform;
    
    if (bKeepWorldTransform)
    {
        const FTransform PreviousWorldTransform = GetWorldTransform();
        FMatrix NewRelativeMatrix = PreviousWorldTransform.GetLocalMatrix();
        
        if (InParent != nullptr)
        {
            const FMatrix NewParentWorldMatrix = InParent->GetWorldMatrix();
            const float Determinant = NewParentWorldMatrix.Determinant();
            
            if (!std::isfinite(Determinant) || std::fabs(Determinant) <= 1.0e-8f)
            {
                return false;
            }
            NewRelativeMatrix = NewRelativeMatrix * NewParentWorldMatrix.Invert();
        }
        
        if (!TryMakeTransform(NewRelativeMatrix, NewRelativeTransform))
        {
            return false;
        }
    }
    
    if (Parent != nullptr)
    {
        Parent->Children.RemoveSingle(this);
    }
    
    Parent = InParent;
    
    if (Parent != nullptr)
    {
        Parent->Children.AddUnique(this);
    }
    
    Transform = NewRelativeTransform;
    return true;
}

void USceneComponent::DetachFromComponent(bool bKeepWorldTransform)
{
    if (Parent == nullptr)
    {
        return;
    }

    FTransform PreviousWorldTransform;

    if (bKeepWorldTransform)
    {
        PreviousWorldTransform = GetWorldTransform();
    }

    Parent->Children.RemoveSingle(this);
    Parent = nullptr;

    if (bKeepWorldTransform)
    {
        Transform = PreviousWorldTransform;
    }
}