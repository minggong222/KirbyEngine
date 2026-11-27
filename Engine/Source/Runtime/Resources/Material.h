#pragma once

#include <cmath>

#include "Core/Math/Math.h"
#include "Asset.h"
#include "Resources/Texture2D.h"
#include "UObject/ObjectMacros.h"
#include "Resources/Material.generated.h"

UCLASS()
class UMaterial : public UAsset
{
    GENERATED_BODY()

public:
    void SetBaseColorTexture(UTexture2D* InTexture)
    {
        BaseColorTexture = InTexture;
    }

    UTexture2D* GetBaseColorTexture() const
    {
        return BaseColorTexture;
    }

    void SetBaseColor(const FVector4& InColor)
    {
        BaseColor = InColor;
    }

    const FVector4& GetBaseColor() const
    {
        return BaseColor;
    }

    void SetUVScrollSpeed(const FVector2D& InSpeed)
    {
        UVScrollSpeed = InSpeed;
    }

    const FVector2D& GetUVScrollSpeed() const
    {
        return UVScrollSpeed;
    }

    void SetUVOffset(const FVector2D& InOffset)
    {
        UVOffset = InOffset;
    }

    const FVector2D& GetUVOffset() const
    {
        return UVOffset;
    }

    void TickUVScroll(float DeltaTime)
    {
        if (!std::isfinite(DeltaTime) || DeltaTime <= 0.0f)
        {
            return;
        }

        if (!std::isfinite(UVScrollSpeed.x) || !std::isfinite(UVScrollSpeed.y))
        {
            return;
        }

        UVOffset.x = std::fmod(UVOffset.x + UVScrollSpeed.x * DeltaTime, 1.0f);
        UVOffset.y = std::fmod(UVOffset.y + UVScrollSpeed.y * DeltaTime, 1.0f);

        if (UVOffset.x < 0.0f) UVOffset.x += 1.0f;
        if (UVOffset.y < 0.0f) UVOffset.y += 1.0f;
    }

    void GetDependencies(TArray<UAsset*>& OutDependencies) const override
    {
        OutDependencies.Reset();

        if (BaseColorTexture != nullptr)
        {
            OutDependencies.Add(static_cast<UAsset*>(BaseColorTexture));
        }
    }

    bool SupportsPackage() const override { return true; }
    void Serialize(FArchive& Ar) override;
    bool PostLoad(FAssetManager& AssetManager, FRHI& RHI) override;

private:
    // Texture asset path read by Serialize, resolved by PostLoad.
    FString PendingBaseColorTexturePath;

    UTexture2D* BaseColorTexture = nullptr;
    FVector4 BaseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    FVector2D UVScrollSpeed = FVector2D(0.0f, 0.0f);
    FVector2D UVOffset = FVector2D(0.0f, 0.0f);
};