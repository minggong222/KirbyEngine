#include "Core/Precompiled.h"
#include "Material.h"

#include "Resources/AssetManager.h"
#include "Serialization/Archive.h"

void UMaterial::Serialize(FArchive& Ar)
{
    FString TexturePath = BaseColorTexture != nullptr ? BaseColorTexture->GetAssetPath() : FString();

    Ar << BaseColor << UVScrollSpeed << TexturePath;

    if (Ar.IsLoading())
    {
        PendingBaseColorTexturePath = TexturePath;
        UVOffset = FVector2D(0.0f, 0.0f);
    }
}

bool UMaterial::PostLoad(FAssetManager& AssetManager, FRHI& RHI)
{
    (void)RHI;

    BaseColorTexture = PendingBaseColorTexturePath.IsEmpty() ? nullptr : AssetManager.Find<UTexture2D>(PendingBaseColorTexturePath);
    PendingBaseColorTexturePath = FString();
    return true;
}
