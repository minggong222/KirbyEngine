#include "RHI/RHI.h"

#include "Resources/TextureResource.h"

FRHI* GRHI = nullptr;

void FTexture2DResource::Release()
{
    if (GRHI != nullptr)
    {
        GRHI->ReleaseTexture2D(*this);
    }

    Texture = nullptr;
    ShaderResourceView = nullptr;
    Width = 0;
    Height = 0;
}
