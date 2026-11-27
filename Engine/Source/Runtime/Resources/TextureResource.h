#pragma once

#include "Core/STL.h"
#include "RHI/RHIResources.h"

// GPU side of a 2D texture: backend handles plus size. Created and released through GRHI.
struct FTexture2DResource
{
    FTexture2DResource() = default;
    FTexture2DResource(const FTexture2DResource&) = delete;
    FTexture2DResource& operator=(const FTexture2DResource&) = delete;
    FTexture2DResource(FTexture2DResource&&) = delete;
    FTexture2DResource& operator=(FTexture2DResource&&) = delete;

    FRHITexture* Texture = nullptr;
    FRHIShaderResourceView* ShaderResourceView = nullptr;

    uint32 Width = 0;
    uint32 Height = 0;

    bool IsValid() const
    {
        return Texture != nullptr && ShaderResourceView != nullptr;
    }

    // Frees the GPU objects (through GRHI) and resets the handles.
    void Release();
};
