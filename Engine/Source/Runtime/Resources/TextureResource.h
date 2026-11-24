#pragma once

#include <d3d11.h>

#include "Core/STL.h"

struct FTexture2DResource
{
    FTexture2DResource() = default;
    FTexture2DResource(const FTexture2DResource&) = delete;
    FTexture2DResource& operator=(const FTexture2DResource&) = delete;
    FTexture2DResource(FTexture2DResource&&) = delete;
    FTexture2DResource& operator=(FTexture2DResource&&) = delete;
    
    ID3D11Texture2D* Texture = nullptr;
    ID3D11ShaderResourceView* ShaderResourceView = nullptr;

    uint32 Width = 0;
    uint32 Height = 0;

    bool IsValid() const
    {
        return Texture != nullptr && ShaderResourceView != nullptr;
    }

    void Release()
    {
        if (ShaderResourceView != nullptr)
        {
            ShaderResourceView->Release();
            ShaderResourceView = nullptr;
        }

        if (Texture != nullptr)
        {
            Texture->Release();
            Texture = nullptr;
        }

        Width = 0;
        Height = 0;
    }
};