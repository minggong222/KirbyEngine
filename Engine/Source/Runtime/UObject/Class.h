#pragma once
#include "Object.h"
#include "Core/Math/Transform.h"

enum class EPropertyType : uint8
{
    Unknown,
    Bool,
    Int32,
    UInt32,
    Float,
    Double,
    String,
    Vector,
    Rotator,
    Transform,
    Struct
};

enum EPropertyFlahs : uint32
{
    Property_None = 0,
    Property_Visible = 1u << 0,
    Property_Editable = 1u << 1
};

class FTypeInfo;

class FProperty
{
public:
    FProperty(const char* InName, EPropertyType InType, SIZE_T InSize, uint32 InFlags = Property_Visible | Property_Editable, const char* InDisplayName = nullptr, const char* InCategory = "Default", const FTypeInfo* InValueTypeInfo = nullptr);
    
    virtual ~FProperty() = default;
    
    const FString& GetName() const { return Name; }
    const FString& GetDisplayName() const { return DisplayName; }
    const FString& GetCategory() const { return Category; }
    
    EPropertyType GetType() const { return Type; }
    SIZE_T GetSize() const { return Size; }
    uint32 GetFlags() const { return Flags; }
    
    bool IsVisible() const { return (Flags & Property_Visible) != 0; }
    bool IsEditable() const { return (Flags & Property_Editable) != 0; }
    const FTypeInfo* GetValueTypeInfo() const { return ValueTypeInfo; }
    
    virtual void* GetValuePtr(void* Instance) const = 0;
    virtual const void* GetValuePtr(const void* Instance) const = 0;
    virtual bool SetValue(void* Instance, EPropertyType ValueType, const void* Value) const = 0;
    
private:
    FString Name;
    FString DisplayName;
    FString Category;
    EPropertyType Type;
    SIZE_T Size;
    uint32 Flags;
    const FTypeInfo* ValueTypeInfo;
};

class FTypeInfo
{
public:
    FTypeInfo(const char* InName, SIZE_T InSize, const FTypeInfo* InSuperType = nullptr);
    virtual ~FTypeInfo();
    
    const FString& GetName() const { return Name; }
    SIZE_T GetSize() const { return Size; }

    const FTypeInfo* GetSuperType() const { return SuperType; }
    bool IsChildOf(const FTypeInfo* InType) const;
    void AddProperty(FProperty* Property);
    const FProperty* FindProperty(const char* InName) const;
    void GetProperties(TArray<const FProperty*>& OutProperties, bool bIncludeSuper = true) const;
private:
    FString Name;
    SIZE_T Size;
    
    const FTypeInfo* SuperType;
    TArray<FProperty*> Properties;
};

template<typename T>
struct TPropertyTypeTraits
{
    static constexpr EPropertyType Value = EPropertyType::Struct;
};

template<>
struct TPropertyTypeTraits<bool>
{
    static constexpr EPropertyType Value = EPropertyType::Bool;
};

template<>
struct TPropertyTypeTraits<int32>
{
    static constexpr EPropertyType Value = EPropertyType::Int32;
};

template<>
struct TPropertyTypeTraits<uint32>
{
    static constexpr EPropertyType Value = EPropertyType::UInt32;
};

template<>
struct TPropertyTypeTraits<float>
{
    static constexpr EPropertyType Value = EPropertyType::Float;
};

template<>
struct TPropertyTypeTraits<double>
{
    static constexpr EPropertyType Value = EPropertyType::Double;
};

template<>
struct TPropertyTypeTraits<FString>
{
    static constexpr EPropertyType Value = EPropertyType::String;
};

template<>
struct TPropertyTypeTraits<FVector>
{
    static constexpr EPropertyType Value = EPropertyType::Vector;
};

template<>
struct TPropertyTypeTraits<FRotator>
{
    static constexpr EPropertyType Value = EPropertyType::Rotator;
};

template<>
struct TPropertyTypeTraits<FTransform>
{
    static constexpr EPropertyType Value = EPropertyType::Transform;
};

template<typename TOwner, typename TValue>
class TMemberProperty final : public FProperty
{
public:
    using FMemberPointer = TValue TOwner::*;
    
    TMemberProperty(const char* InName, FMemberPointer InMember, uint32 InFlags = Property_Visible | Property_Editable, const char* InDisplayName = nullptr, const char* InCategory = "Default", const FTypeInfo* InValueTypeInfo = nullptr)
        : FProperty(InName, TPropertyTypeTraits<TValue>::Value, sizeof(TValue), InFlags, InDisplayName, InCategory, InValueTypeInfo), MemberPointer(InMember) {}
    
    void* GetValuePtr(void* Instance) const override
    {
        TOwner* Owner = ResolveOwner(Instance);
        return Owner != nullptr ? &(Owner->*MemberPointer) : nullptr;
    }
    
    const void* GetValuePtr(const void* Instance) const override
    {
        const TOwner* Owner = ResolveOwner(Instance);
        return Owner != nullptr ? &(Owner->*MemberPointer) : nullptr;
    }
    
    bool SetValue(void* Instance, EPropertyType ValueType, const void* Value) const override
    {
        if (Value == nullptr || ValueType != GetType())
        {
            return false;
        }
        
        if (!IsEditable())
        {
            return false;
        }
        
        TValue* Target = static_cast<TValue*>(GetValuePtr(Instance));
        
        if (Target == nullptr)
        {
            return false;
        }
        
        *Target = *static_cast<const TValue*>(Value);
        
        return true;
    }
private:
    static TOwner* ResolveOwner(void* Instance)
    {
        if (Instance == nullptr)
        {
            return nullptr;
        }
        
        if constexpr (std::is_base_of_v<UObject, TOwner>)
        {
            UObject* Object = static_cast<UObject*>(Instance);
            
            if (!Object->IsA(TOwner::StaticClass()))
            {
                return nullptr;
            }
            
            return static_cast<TOwner*>(Object);
        }
        else
        {
            return static_cast<TOwner*>(Instance);
        }
    }
    
    static const TOwner* ResolveOwner(const void* Instance)
    {
        if (Instance == nullptr)
        {
            return nullptr;
        }
        
        if constexpr (std::is_base_of_v<UObject, TOwner>)
        {
            const UObject* Object = static_cast<const UObject*>(Instance);
            
            if (!Object->IsA(TOwner::StaticClass()))
            {
                return nullptr;
            }
            
            return static_cast<const TOwner*>(Object);
        }
        else
        {
            return static_cast<const TOwner*>(Instance);
        }
    }
    FMemberPointer MemberPointer;
};

template<typename TOwner, typename TValue>
FProperty* MakeProperty(const char* Name, TValue TOwner::* Member, uint32 Flags = Property_Visible | Property_Editable, const char* DisplayName = nullptr, const char* Category = nullptr, const FTypeInfo* ValueTypeInfo = nullptr)
{
    return new TMemberProperty<TOwner, TValue>(Name, Member, Flags, DisplayName, Category, ValueTypeInfo);
}

class UClass : public UObject, public FTypeInfo
{
public:
    UClass(const char* InName, const UClass* InSuperClass, SIZE_T InClassSize);
    
    static UClass* StaticClass();
    UClass* GetClass() const override;
};
template<typename T>
T* Cast(UObject* Object)
{
    return Object != nullptr && Object->IsA<T>()
        ? static_cast<T*>(Object)
        : nullptr;
}
// 클래스 선언부에서 사용
#define DECLARE_CLASS(ClassName, SuperClassName)                    \
public:                                                             \
    static UClass* StaticClass();                                   \
    UClass* GetClass() const override;                              \
    static void RegisterProperties(UClass& ClassInfo);

// 프로퍼티를 직접 등록
#define REGISTER_PROPERTY(ClassInfo, OwnerType, MemberName)         \
    (ClassInfo).AddProperty(                                        \
        MakeProperty(#MemberName, &OwnerType::MemberName)           \
    )

// 표시 이름, 카테고리, 편집 가능 여부까지 지정
#define REGISTER_PROPERTY_META(                                     \
    ClassInfo, OwnerType, MemberName, DisplayName, Category, Flags  \
)                                                                   \
    (ClassInfo).AddProperty(                                        \
        MakeProperty(                                               \
            #MemberName,                                            \
            &OwnerType::MemberName,                                 \
            Flags,                                                  \
            DisplayName,                                            \
            Category                                                \
        )                                                           \
    )

// 클래스 구현부에서 사용
#define IMPLEMENT_CLASS(ClassName, SuperClassName)                  \
UClass* ClassName::StaticClass()                                    \
{                                                                   \
    static UClass* ClassInfo = new UClass(                          \
        #ClassName,                                                 \
        SuperClassName::StaticClass(),                              \
        sizeof(ClassName)                                           \
    );                                                              \
    static const bool bRegistered = []()                            \
    {                                                               \
        ClassName::RegisterProperties(*ClassInfo);                  \
        return true;                                                \
    }();                                                            \
    (void)bRegistered;                                              \
    return ClassInfo;                                               \
}                                                                   \
UClass* ClassName::GetClass() const                                 \
{                                                                   \
    return ClassName::StaticClass();                                \
}

#define IMPLEMENT_CLASS_NO_PROPERTIES(ClassName, SuperClassName)    \
IMPLEMENT_CLASS(ClassName, SuperClassName)                          \
void ClassName::RegisterProperties(UClass&) {}
   