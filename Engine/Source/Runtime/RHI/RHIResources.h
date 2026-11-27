#pragma once

#include "Core/STL.h"

// Backend-neutral GPU resource handles.
//
// These types are intentionally never defined: code outside the RHI backend only
// passes the pointers around. Each backend maps them to its native objects
// (the D3D11 backend stores ID3D11Buffer / ID3D11Texture2D / ID3D11ShaderResourceView).
struct FRHIBuffer;
struct FRHITexture;
struct FRHIShaderResourceView;

enum class ERHIPrimitiveTopology : uint8
{
    TriangleList,
    LineList
};
