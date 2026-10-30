#include "SceneComponent.h"

IMPLEMENT_CLASS(USceneComponent, UObject)

void USceneComponent::RegisterProperties(UClass& ClassInfo)
{
    REGISTER_PROPERTY_META(ClassInfo, USceneComponent, Transform, "Transform", "Transform", Property_Visible | Property_Editable);
}
