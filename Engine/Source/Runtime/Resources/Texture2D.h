#pragma once

#include "Asset.h"
#include "TextureResource.h"
#include "UObject/ObjectMacros.h"
#include "Resources/Texture2D.generated.h"

#include <filesystem>

class FRHI;

UCLASS()
class UTexture2D : public UAsset
{
    GENERATED_BODY()

public:
    bool Initialize(FRHI& Renderer, const std::filesystem::path& SourcePath);
    bool Reload(FRHI& Renderer, const std::filesystem::path& SourcePath);

    uint32 GetSizeX() const { return Resource.Width; }
    uint32 GetSizeY() const { return Resource.Height; }
    const FTexture2DResource& GetResource() const { return Resource; }
    bool IsValid() const { return Resource.IsValid(); }

    bool SupportsPackage() const override { return true; }
    void Serialize(FArchive& Ar) override;
    bool PostLoad(FAssetManager& AssetManager, FRHI& RHI) override;

    ~UTexture2D() override;

private:
    FTexture2DResource Resource;

    // Decoded RGBA8 pixels, kept so the texture can be written to a package.
    TArray<uint8> SourcePixels;
    uint32 SourceWidth = 0;
    uint32 SourceHeight = 0;
};
