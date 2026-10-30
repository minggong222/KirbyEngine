#include "Class.h"

FProperty::FProperty(const char* InName, EPropertyType InType, SIZE_T InSize, uint32 InFlags, const char* InDisplayName, const char* InCategory, const FTypeInfo* InValueTypeInfo)
    : Name(InName), DisplayName(InDisplayName != nullptr ? InDisplayName : nullptr), Category(InCategory), Type(InType), Size(InSize), Flags(InFlags), ValueTypeInfo(InValueTypeInfo) {}

FTypeInfo::FTypeInfo(const char* InName, SIZE_T InSize, const FTypeInfo* InSuperType)
    : Name(InName), Size(InSize), SuperType(InSuperType){}

FTypeInfo::~FTypeInfo()
{
    for (int32 Index = 0; Index < Properties.Num(); ++Index)
    {
        delete Properties[Index];
    }
    Properties.Reset();
}

bool FTypeInfo::IsChildOf(const FTypeInfo* InType) const
{
    if (InType == nullptr)
    {
        return false;
    }
    for (const FTypeInfo* Current = this; Current != nullptr; Current = Current->SuperType)
    {
        if (Current == InType)
        {
            return true;
        }
    }
    return false;
}

void FTypeInfo::AddProperty(FProperty* Property)
{
    assert(Property != nullptr);
    
    if (Property == nullptr)
    {
        return;
    }
    
    for (int32 Index = 0; Index < Properties.Num(); ++Index)
    {
        assert(Properties[Index]->GetName() != Property->GetName());
    }
    Properties.Add(Property);
}

const FProperty* FTypeInfo::FindProperty(const char* InName) const
{
    if (InName == nullptr)
    {
        return nullptr;
    }
    const FString SearchName(InName);
    
    for (int32 Index = Properties.Num() - 1; Index >= 0; --Index)
    {
        if (Properties[Index]->GetName() == SearchName)
        {
            return Properties[Index];
        }
    }
    
    return SuperType != nullptr ? SuperType->FindProperty(InName) : nullptr;
}

void FTypeInfo::GetProperties(TArray<const FProperty*>& OutProperties, bool bIncludeSuper) const
{
    OutProperties.Reset();
    if (bIncludeSuper && SuperType != nullptr)
    {
        SuperType->GetProperties(OutProperties, true);
    }
    
    for (int32 Index = 0; Index < Properties.Num(); Index++)
    {
        OutProperties.Add(Properties[Index]);
    }
}

UClass::UClass(const char* InName, const UClass* InSuperClass, SIZE_T InClassSize)
    : UObject(), FTypeInfo(InName, InClassSize, InSuperClass)
{
}

UClass* UClass::StaticClass()
{
    static UClass* ClassInfo = new UClass("UClass", UObject::StaticClass(), sizeof(UClass));
    return ClassInfo;
}

UClass* UClass::GetClass() const
{
    return StaticClass();
}
