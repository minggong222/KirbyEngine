#pragma once

#include "Asset.h"
#include "TextureResource.h"
#include "UObject/ObjectMacros.h"
#include "Resources/Texture2D.generated.h"

#include <filesystem>

class URenderer;

UCLASS()
class UTexture2D : public UAsset
{
    GENERATED_BODY()

public:
    bool Initialize(URenderer& Renderer, const std::filesystem::path& SourcePath);
    bool Reload(URenderer& Renderer, const std::filesystem::path& SourcePath);

    uint32 GetSizeX() const { return Resource.Width; }
    uint32 GetSizeY() const { return Resource.Height; }
    const FTexture2DResource& GetResource() const { return Resource; }
    bool IsValid() const { return Resource.IsValid(); }

    ~UTexture2D() override;

private:
    FTexture2DResource Resource;
};
