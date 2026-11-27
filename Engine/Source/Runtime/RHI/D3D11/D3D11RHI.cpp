#include <d3d11.h>
#include <d3dcompiler.h>

#include "RHI/D3D11/D3D11RHI.h"
#include "Resources/TextureResource.h"
#include "Core/Math/Math.h"

#include <cstddef>
#include <cstring>
#include <limits>

// 렌더러 초기화 함수
void FD3D11RHI::Create(HWND hWindow)
{
    bInitializationFailed = false;

    CreateDeviceAndSwapChain(hWindow);

    if (bInitializationFailed)
    {
        return;
    }

    CreateFrameBuffer();

    if (bInitializationFailed)
    {
        return;
    }

    CreateRasterizerState();
}

// Direct3D 장치 및 스왑 체인을 생성하는 함수
void FD3D11RHI::CreateDeviceAndSwapChain(HWND hWindow)
{
    if (hWindow == nullptr)
    {
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] Invalid window handle.\n");
        return;
    }

    D3D_FEATURE_LEVEL FeatureLevels[] = {D3D_FEATURE_LEVEL_11_0};

    DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
    SwapChainDesc.BufferDesc.Width = 0;
    SwapChainDesc.BufferDesc.Height = 0;
    SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    SwapChainDesc.SampleDesc.Count = 1;
    SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    SwapChainDesc.BufferCount = 2;
    SwapChainDesc.OutputWindow = hWindow;
    SwapChainDesc.Windowed = TRUE;
    SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
    DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, DeviceFlags, FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION, &SwapChainDesc, &mSwapChain, &mDevice, nullptr, &mDeviceContext);

#if defined(_DEBUG)
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        if (mDeviceContext != nullptr)
        {
            mDeviceContext->Flush();
            mDeviceContext->Release();
            mDeviceContext = nullptr;
        }

        if (mSwapChain != nullptr)
        {
            mSwapChain->Release();
            mSwapChain = nullptr;
        }

        if (mDevice != nullptr)
        {
            mDevice->Release();
            mDevice = nullptr;
        }

        DeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;

        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, DeviceFlags, FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION, &SwapChainDesc, &mSwapChain, &mDevice, nullptr, &mDeviceContext);
    }
#endif

    if (FAILED(hr))
    {
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] D3D11CreateDeviceAndSwapChain failed.\n");
        return;
    }

    hr = mSwapChain->GetDesc(&SwapChainDesc);

    if (FAILED(hr))
    {
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] IDXGISwapChain::GetDesc failed.\n");
        return;
    }

    mViewportInfo = {0.0f, 0.0f, static_cast<float>(SwapChainDesc.BufferDesc.Width), static_cast<float>(SwapChainDesc.BufferDesc.Height), 0.0f, 1.0f};
}

// Direct3D 장치 및 스왑 체인을 해제하는 함수
void FD3D11RHI::ReleaseDeviceAndSwapChain()
{
    if (mDeviceContext != nullptr)
    {
        mDeviceContext->Flush();
        mDeviceContext->Release();
        mDeviceContext = nullptr;
    }

    if (mSwapChain != nullptr)
    {
        mSwapChain->Release();
        mSwapChain = nullptr;
    }

    if (mDevice != nullptr)
    {
        mDevice->Release();
        mDevice = nullptr;
    }
}

// 프레임 버퍼를 생성하는 함수
void FD3D11RHI::CreateFrameBuffer()
{
    if (mDevice == nullptr || mSwapChain == nullptr)
    {
        bInitializationFailed = true;
        return;
    }

    HRESULT hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&mFrameBuffer));

    if (FAILED(hr) || mFrameBuffer == nullptr)
    {
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] GetBuffer failed.\n");
        return;
    }

    D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
    RTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

    hr = mDevice->CreateRenderTargetView(mFrameBuffer, &RTVDesc, &mFrameBufferRTV);

    if (FAILED(hr))
    {
        bInitializationFailed = true;
        ReleaseFrameBuffer();
        OutputDebugStringA("[Renderer] CreateRenderTargetView failed.\n");
    }
}

// 프레임 버퍼를 해제하는 함수
void FD3D11RHI::ReleaseFrameBuffer()
{
    if (mFrameBufferRTV != nullptr)
    {
        mFrameBufferRTV->Release();
        mFrameBufferRTV = nullptr;
    }

    if (mFrameBuffer != nullptr)
    {
        mFrameBuffer->Release();
        mFrameBuffer = nullptr;
    }
}

// 래스터라이저 상태를 생성하는 함수
void FD3D11RHI::CreateRasterizerState()
{
    if (mDevice == nullptr)
    {
        bInitializationFailed = true;
        return;
    }

    D3D11_RASTERIZER_DESC RasterizerDesc = {};
    RasterizerDesc.FillMode = D3D11_FILL_SOLID;
    RasterizerDesc.CullMode = D3D11_CULL_BACK;
    RasterizerDesc.FrontCounterClockwise = FALSE;
    RasterizerDesc.DepthClipEnable = TRUE;

    HRESULT hr = mDevice->CreateRasterizerState(&RasterizerDesc, &mRasterizerState);

    if (FAILED(hr))
    {
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] CreateRasterizerState failed.\n");
        return;
    }

    D3D11_RASTERIZER_DESC OutlineRasterizerDesc = RasterizerDesc;
    OutlineRasterizerDesc.CullMode = D3D11_CULL_FRONT;

    hr = mDevice->CreateRasterizerState(&OutlineRasterizerDesc, &mOutlineRasterizerState);

    if (FAILED(hr))
    {
        bInitializationFailed = true;
        ReleaseRasterizerState();
        OutputDebugStringA("[Renderer] CreateOutlineRasterizerState failed.\n");
        return;
    }

    D3D11_RASTERIZER_DESC WireframeDesc = RasterizerDesc;
    WireframeDesc.FillMode = D3D11_FILL_WIREFRAME;

    hr = mDevice->CreateRasterizerState(&WireframeDesc, &mWireframeRasterizerState);

    if (FAILED(hr))
    {
        ReleaseRasterizerState();
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] CreateWireframeRasterizerState failed.\n");
        return;
    }

    D3D11_BLEND_DESC BlendDesc = {};
    BlendDesc.RenderTarget[0].BlendEnable = TRUE;
    BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = mDevice->CreateBlendState(&BlendDesc, &mAlphaBlendState);

    if (FAILED(hr))
    {
        ReleaseRasterizerState();
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] CreateAlphaBlendState failed.\n");
    }
}

// 래스터라이저 상태를 해제하는 함수
void FD3D11RHI::ReleaseRasterizerState()
{
    if (mAlphaBlendState != nullptr)
    {
        mAlphaBlendState->Release();
        mAlphaBlendState = nullptr;
    }

    if (mWireframeRasterizerState != nullptr)
    {
        mWireframeRasterizerState->Release();
        mWireframeRasterizerState = nullptr;
    }

    if (mOutlineRasterizerState != nullptr)
    {
        mOutlineRasterizerState->Release();
        mOutlineRasterizerState = nullptr;
    }

    if (mRasterizerState != nullptr)
    {
        mRasterizerState->Release();
        mRasterizerState = nullptr;
    }
}

// 렌더러에 사용된 모든 리소스를 해제하는 함수
void FD3D11RHI::Release()
{
    if (mDeviceContext != nullptr)
    {
        mDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
    }

    ReleaseDynamicVertexBuffer();
    ReleaseDynamicIndexBuffer();
    ReleaseRasterizerState();
    ReleaseFrameBuffer();
    ReleaseDeviceAndSwapChain();
}

// 스왑 체인의 백 버퍼와 프론트 버퍼를 교체하여 화면에 출력
void FD3D11RHI::SwapBuffer()
{
    if (mSwapChain == nullptr)
    {
        return;
    }

    const HRESULT hr = mSwapChain->Present(0, 0);

    if (FAILED(hr))
    {
        OutputDebugStringA("[Renderer] Present failed.\n");
    }
}

void FD3D11RHI::CreateShader()
{
    if (mDevice == nullptr)
    {
        bInitializationFailed = true;
        return;
    }

    ID3DBlob* VertexShaderBlob = nullptr;
    ID3DBlob* PixelShaderBlob = nullptr;
    ID3DBlob* ErrorBlob = nullptr;

    HRESULT hr = D3DCompileFromFile(L"Engine/Resources/Shaders/shaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VertexShaderBlob, &ErrorBlob);

    if (FAILED(hr))
    {
        if (ErrorBlob != nullptr)
        {
            OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
            ErrorBlob->Release();
        }

        bInitializationFailed = true;
        return;
    }

    if (ErrorBlob != nullptr)
    {
        ErrorBlob->Release();
        ErrorBlob = nullptr;
    }

    hr = D3DCompileFromFile(L"Engine/Resources/Shaders/shaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PixelShaderBlob, &ErrorBlob);

    if (FAILED(hr))
    {
        if (ErrorBlob != nullptr)
        {
            OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
            ErrorBlob->Release();
        }

        VertexShaderBlob->Release();
        bInitializationFailed = true;
        return;
    }

    if (ErrorBlob != nullptr)
    {
        ErrorBlob->Release();
        ErrorBlob = nullptr;
    }

    hr = mDevice->CreateVertexShader(VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), nullptr, &mSimpleVertexShader);

    if (FAILED(hr))
    {
        VertexShaderBlob->Release();
        PixelShaderBlob->Release();
        bInitializationFailed = true;
        return;
    }

    hr = mDevice->CreatePixelShader(PixelShaderBlob->GetBufferPointer(), PixelShaderBlob->GetBufferSize(), nullptr, &mSimplePixelShader);

    if (FAILED(hr))
    {
        VertexShaderBlob->Release();
        PixelShaderBlob->Release();
        ReleaseShader();
        bInitializationFailed = true;
        return;
    }

    D3D11_INPUT_ELEMENT_DESC Layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(FVertexSimple, nx)), D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    hr = mDevice->CreateInputLayout(Layout, ARRAYSIZE(Layout), VertexShaderBlob->GetBufferPointer(), VertexShaderBlob->GetBufferSize(), &mSimpleInputLayout);

    VertexShaderBlob->Release();
    PixelShaderBlob->Release();

    if (FAILED(hr))
    {
        ReleaseShader();
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] CreateInputLayout failed.\n");
        return;
    }

    D3D11_SAMPLER_DESC SamplerDesc = {};
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0.0f;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = mDevice->CreateSamplerState(&SamplerDesc, &mTextureSamplerState);

    if (FAILED(hr))
    {
        ReleaseShader();
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] CreateSamplerState failed.\n");
        return;
    }

    mStride = sizeof(FVertexSimple);
}

void FD3D11RHI::ReleaseShader()
{
    if (mSimpleInputLayout != nullptr)
    {
        mSimpleInputLayout->Release();
        mSimpleInputLayout = nullptr;
    }

    if (mSimplePixelShader != nullptr)
    {
        mSimplePixelShader->Release();
        mSimplePixelShader = nullptr;
    }

    if (mSimpleVertexShader != nullptr)
    {
        mSimpleVertexShader->Release();
        mSimpleVertexShader = nullptr;
    }

    if (mTextureSamplerState != nullptr)
    {
        mTextureSamplerState->Release();
        mTextureSamplerState = nullptr;
    }
}

void FD3D11RHI::Prepare()
{
    if (mDeviceContext == nullptr || mFrameBufferRTV == nullptr || mRasterizerState == nullptr)
    {
        return;
    }

    mDeviceContext->ClearRenderTargetView(mFrameBufferRTV, mClearColor);
    mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mDeviceContext->RSSetViewports(1, &mViewportInfo);
    mDeviceContext->RSSetState(bWireframeEnabled ? mWireframeRasterizerState : mRasterizerState);
    mDeviceContext->OMSetRenderTargets(1, &mFrameBufferRTV, nullptr);
    mDeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void FD3D11RHI::PrepareShader()
{
    if (mDeviceContext == nullptr || mSimpleVertexShader == nullptr || mSimplePixelShader == nullptr || mSimpleInputLayout == nullptr)
    {
        return;
    }

    mDeviceContext->VSSetShader(mSimpleVertexShader, nullptr, 0);
    mDeviceContext->PSSetShader(mSimplePixelShader, nullptr, 0);
    mDeviceContext->IASetInputLayout(mSimpleInputLayout);

    if (mConstantBuffer != nullptr)
    {
        mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
        mDeviceContext->PSSetConstantBuffers(0, 1, &mConstantBuffer);
    }
}

void FD3D11RHI::RenderPrimitiveNative(ID3D11Buffer* VertexBuffer, ID3D11Buffer* IndexBuffer, UINT IndexCount, bool bRenderOutline, ID3D11ShaderResourceView* TextureSRV, UINT StartIndexLocation)
{
    if (mDeviceContext == nullptr || VertexBuffer == nullptr || IndexBuffer == nullptr || IndexCount == 0 || mRasterizerState == nullptr || (bRenderOutline && mOutlineRasterizerState == nullptr))
    {
        return;
    }

    if (bRenderOutline)
    {
        mDeviceContext->RSSetState(mOutlineRasterizerState);
    }

    UINT Offset = 0;

    mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mDeviceContext->IASetVertexBuffers(0, 1, &VertexBuffer, &mStride, &Offset);
    mDeviceContext->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    mDeviceContext->PSSetShaderResources(0, 1, &TextureSRV);
    mDeviceContext->PSSetSamplers(0, 1, &mTextureSamplerState);
    mDeviceContext->DrawIndexed(IndexCount, StartIndexLocation, 0);

    if (bRenderOutline)
    {
        mDeviceContext->RSSetState(bWireframeEnabled ? mWireframeRasterizerState : mRasterizerState);
    }
}

void FD3D11RHI::SetViewMode(EViewMode InViewMode)
{
    bWireframeEnabled = InViewMode == EViewMode::Wireframe;
    bUnlitEnabled = InViewMode == EViewMode::Unlit;

    if (mDeviceContext != nullptr)
    {
        mDeviceContext->RSSetState(bWireframeEnabled ? mWireframeRasterizerState : mRasterizerState);
    }
}

ID3D11Buffer* FD3D11RHI::CreateVertexBufferNative(FVertexSimple* vertices, UINT byteWidth)
{
    if (mDevice == nullptr || vertices == nullptr || byteWidth == 0)
    {
        return nullptr;
    }

    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = {};
    vertexbufferSRD.pSysMem = vertices;

    ID3D11Buffer* vertexBuffer = nullptr;

    const HRESULT hr = mDevice->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

    if (FAILED(hr))
    {
        return nullptr;
    }

    return vertexBuffer;
}

void FD3D11RHI::ReleaseVertexBufferNative(ID3D11Buffer* VertexBuffer)
{
    if (VertexBuffer != nullptr)
    {
        VertexBuffer->Release();
    }
}

ID3D11Buffer* FD3D11RHI::CreateIndexBufferNative(const uint32* Indices, UINT IndexCount)
{
    if (mDevice == nullptr || Indices == nullptr || IndexCount == 0)
    {
        return nullptr;
    }

    const uint64 ByteWidth = sizeof(uint32) * static_cast<uint64>(IndexCount);

    if (ByteWidth > (std::numeric_limits<UINT>::max)())
    {
        return nullptr;
    }

    D3D11_BUFFER_DESC IndexBufferDesc = {};
    IndexBufferDesc.ByteWidth = static_cast<UINT>(ByteWidth);
    IndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA IndexBufferData = {};
    IndexBufferData.pSysMem = Indices;

    ID3D11Buffer* IndexBuffer = nullptr;

    const HRESULT hr = mDevice->CreateBuffer(&IndexBufferDesc, &IndexBufferData, &IndexBuffer);

    if (FAILED(hr))
    {
        return nullptr;
    }

    return IndexBuffer;
}

void FD3D11RHI::ReleaseIndexBufferNative(ID3D11Buffer* IndexBuffer)
{
    if (IndexBuffer != nullptr)
    {
        IndexBuffer->Release();
    }
}

bool FD3D11RHI::CreateTexture2D(const uint8* Pixels, uint32 Width, uint32 Height, FTexture2DResource& OutResource)
{
    if (mDevice == nullptr || Pixels == nullptr || Width == 0 || Height == 0 || OutResource.Texture != nullptr || OutResource.ShaderResourceView != nullptr)
    {
        return false;
    }

    if (Width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || Height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
    {
        return false;
    }

    const uint64 RowPitch = static_cast<uint64>(Width) * 4;
    const uint64 PixelByteWidth = RowPitch * Height;

    if (RowPitch > (std::numeric_limits<UINT>::max)() || PixelByteWidth > (std::numeric_limits<UINT>::max)())
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width = Width;
    TextureDesc.Height = Height;
    TextureDesc.MipLevels = 1;
    TextureDesc.ArraySize = 1;
    TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.SampleDesc.Count = 1;
    TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA InitialData = {};
    InitialData.pSysMem = Pixels;
    InitialData.SysMemPitch = static_cast<UINT>(RowPitch);

    ID3D11Texture2D* NewTexture = nullptr;

    HRESULT hr = mDevice->CreateTexture2D(&TextureDesc, &InitialData, &NewTexture);

    if (FAILED(hr) || NewTexture == nullptr)
    {
        return false;
    }

    ID3D11ShaderResourceView* NewShaderResourceView = nullptr;

    hr = mDevice->CreateShaderResourceView(NewTexture, nullptr, &NewShaderResourceView);

    if (FAILED(hr) || NewShaderResourceView == nullptr)
    {
        NewTexture->Release();
        return false;
    }

    OutResource.Texture = reinterpret_cast<FRHITexture*>(NewTexture);
    OutResource.ShaderResourceView = reinterpret_cast<FRHIShaderResourceView*>(NewShaderResourceView);
    OutResource.Width = Width;
    OutResource.Height = Height;

    return true;
}

void FD3D11RHI::CreateConstantBuffer()
{
    if (mDevice == nullptr)
    {
        bInitializationFailed = true;
        return;
    }

    D3D11_BUFFER_DESC ConstantBufferDesc = {};
    ConstantBufferDesc.ByteWidth = (sizeof(FConstants) + 15u) & ~15u;
    ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    const HRESULT hr = mDevice->CreateBuffer(&ConstantBufferDesc, nullptr, &mConstantBuffer);

    if (FAILED(hr))
    {
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] CreateConstantBuffer failed.\n");
    }
}

void FD3D11RHI::ReleaseConstantBuffer()
{
    if (mConstantBuffer != nullptr)
    {
        mConstantBuffer->Release();
        mConstantBuffer = nullptr;
    }
}

void FD3D11RHI::UpdateConstant(FMatrix World, FMatrix View, FMatrix Projection, bool bIsOutline, bool bIsTextured, bool bForceUnlit, const FVector4& InMaterialColor, const FVector2D& InUVOffset, bool bUseVertexNormals)
{
    if (mConstantBuffer == nullptr || mDeviceContext == nullptr)
    {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE MappedResource = {};

    const HRESULT hr = mDeviceContext->Map(mConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

    if (FAILED(hr) || MappedResource.pData == nullptr)
    {
        OutputDebugStringA("[Renderer] Constant buffer Map failed.\n");
        return;
    }

    FConstants* Constants = static_cast<FConstants*>(MappedResource.pData);
    Constants->World = World;
    Constants->View = View;
    Constants->Projection = Projection;
    Constants->NormalMatrix = World.GetNormalMatrix();
    Constants->MaterialColor = InMaterialColor;
    Constants->UVOffset = InUVOffset;
    Constants->MaterialPadding[0] = 0;
    Constants->MaterialPadding[1] = 0;
    Constants->IsOutline = bIsOutline ? 1u : 0u;
    Constants->IsTextured = bIsTextured ? 1u : 0u;
    Constants->IsUnlit = (bUnlitEnabled || bForceUnlit) ? 1u : 0u;
    Constants->UseVertexNormals = bUseVertexNormals ? 1u : 0u;

    mDeviceContext->Unmap(mConstantBuffer, 0);
}

ID3D11Device* FD3D11RHI::GetDeviceNative()
{
    return mDevice;
}

ID3D11DeviceContext* FD3D11RHI::GetDeviceContextNative()
{
    return mDeviceContext;
}

void FD3D11RHI::ResizeBackBuffer(uint32 Width, uint32 Height)
{
    if (Width == 0 || Height == 0 || mDeviceContext == nullptr || mSwapChain == nullptr)
    {
        return;
    }

    if (mViewportInfo.Width == static_cast<float>(Width) && mViewportInfo.Height == static_cast<float>(Height))
    {
        return;
    }

    mDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    mDeviceContext->PSSetShaderResources(0, 1, &NullSRV);

    ReleaseFrameBuffer();

    const HRESULT hr = mSwapChain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0);

    if (FAILED(hr))
    {
        OutputDebugStringA("[Renderer] ResizeBuffers failed.\n");
        CreateFrameBuffer();
        return;
    }

    mViewportInfo = {0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f};

    CreateFrameBuffer();

    if (mFrameBufferRTV == nullptr)
    {
        OutputDebugStringA("[Renderer] Back buffer recreation failed.\n");
    }
}

void FD3D11RHI::SetViewportRegion(float X, float Y, float Width, float Height)
{
    if (mDeviceContext == nullptr || ViewportWidth == 0 || ViewportHeight == 0 || !std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Width) || !std::isfinite(Height) || Width <= 0.0f || Height <= 0.0f)
    {
        return;
    }

    if (X < 0.0f || Y < 0.0f || X + Width > static_cast<float>(ViewportWidth) || Y + Height > static_cast<float>(ViewportHeight))
    {
        return;
    }

    D3D11_VIEWPORT Viewport = {};
    Viewport.TopLeftX = X;
    Viewport.TopLeftY = Y;
    Viewport.Width = Width;
    Viewport.Height = Height;
    Viewport.MinDepth = 0.0f;
    Viewport.MaxDepth = 1.0f;

    mDeviceContext->RSSetViewports(1, &Viewport);
}

void FD3D11RHI::CreateViewportRenderTarget(uint32 Width, uint32 Height)
{
    auto Fail = [this]()
    {
        ReleaseViewportRenderTarget();
        bInitializationFailed = true;
        OutputDebugStringA("[Renderer] Viewport render target creation failed.\n");
    };

    if (mDevice == nullptr || mDeviceContext == nullptr || Width == 0 || Height == 0)
    {
        bInitializationFailed = true;
        return;
    }

    D3D11_TEXTURE2D_DESC ColorDesc = {};
    ColorDesc.Width = Width;
    ColorDesc.Height = Height;
    ColorDesc.MipLevels = 1;
    ColorDesc.ArraySize = 1;
    ColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ColorDesc.SampleDesc.Count = 1;
    ColorDesc.Usage = D3D11_USAGE_DEFAULT;
    ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = mDevice->CreateTexture2D(&ColorDesc, nullptr, &ViewportTexture);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    hr = mDevice->CreateRenderTargetView(ViewportTexture, nullptr, &ViewportRTV);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    hr = mDevice->CreateShaderResourceView(ViewportTexture, nullptr, &ViewportSRV);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    D3D11_TEXTURE2D_DESC DepthDesc = {};
    DepthDesc.Width = Width;
    DepthDesc.Height = Height;
    DepthDesc.MipLevels = 1;
    DepthDesc.ArraySize = 1;
    DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthDesc.SampleDesc.Count = 1;
    DepthDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = mDevice->CreateTexture2D(&DepthDesc, nullptr, &ViewportDepthTexture);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    hr = mDevice->CreateDepthStencilView(ViewportDepthTexture, nullptr, &ViewportDSV);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    D3D11_DEPTH_STENCIL_DESC StateDesc = {};
    StateDesc.DepthEnable = TRUE;
    StateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    StateDesc.DepthFunc = D3D11_COMPARISON_LESS;

    hr = mDevice->CreateDepthStencilState(&StateDesc, &ViewportDepthState);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    D3D11_DEPTH_STENCIL_DESC OverlayDepthDesc = StateDesc;
    OverlayDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    OverlayDepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

    hr = mDevice->CreateDepthStencilState(&OverlayDepthDesc, &ViewportOverlayDepthState);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    ViewportRenderInfo = {0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f};
    ViewportWidth = Width;
    ViewportHeight = Height;
}

void FD3D11RHI::ResizeViewportRenderTarget(uint32 Width, uint32 Height)
{
    if (Width == 0 || Height == 0)
    {
        return;
    }

    if (ViewportTexture != nullptr && ViewportWidth == Width && ViewportHeight == Height)
    {
        return;
    }

    ReleaseViewportRenderTarget();
    CreateViewportRenderTarget(Width, Height);
}

void FD3D11RHI::CreateAssetPreviewRenderTarget(uint32 Width, uint32 Height)
{
    if (AssetPreviewTexture != nullptr || mDevice == nullptr || mDeviceContext == nullptr || Width == 0 || Height == 0)
    {
        return;
    }

    const auto Fail = [this]()
    {
        ReleaseAssetPreviewRenderTarget();
        OutputDebugStringA("[Renderer] Asset preview render target creation failed.\n");
    };

    D3D11_TEXTURE2D_DESC ColorDesc = {};
    ColorDesc.Width = Width;
    ColorDesc.Height = Height;
    ColorDesc.MipLevels = 1;
    ColorDesc.ArraySize = 1;
    ColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ColorDesc.SampleDesc.Count = 1;
    ColorDesc.Usage = D3D11_USAGE_DEFAULT;
    ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = mDevice->CreateTexture2D(&ColorDesc, nullptr, &AssetPreviewTexture);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    hr = mDevice->CreateRenderTargetView(AssetPreviewTexture, nullptr, &AssetPreviewRTV);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    hr = mDevice->CreateShaderResourceView(AssetPreviewTexture, nullptr, &AssetPreviewSRV);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    D3D11_TEXTURE2D_DESC DepthDesc = {};
    DepthDesc.Width = Width;
    DepthDesc.Height = Height;
    DepthDesc.MipLevels = 1;
    DepthDesc.ArraySize = 1;
    DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    DepthDesc.SampleDesc.Count = 1;
    DepthDesc.Usage = D3D11_USAGE_DEFAULT;
    DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    hr = mDevice->CreateTexture2D(&DepthDesc, nullptr, &AssetPreviewDepthTexture);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    hr = mDevice->CreateDepthStencilView(AssetPreviewDepthTexture, nullptr, &AssetPreviewDSV);

    if (FAILED(hr))
    {
        Fail();
        return;
    }

    AssetPreviewRenderInfo.TopLeftX = 0.0f;
    AssetPreviewRenderInfo.TopLeftY = 0.0f;
    AssetPreviewRenderInfo.Width = static_cast<float>(Width);
    AssetPreviewRenderInfo.Height = static_cast<float>(Height);
    AssetPreviewRenderInfo.MinDepth = 0.0f;
    AssetPreviewRenderInfo.MaxDepth = 1.0f;

    AssetPreviewWidth = Width;
    AssetPreviewHeight = Height;
}

void FD3D11RHI::BeginAssetPreviewRender()
{
    if (mDeviceContext == nullptr || AssetPreviewRTV == nullptr || AssetPreviewDSV == nullptr || ViewportDepthState == nullptr || mRasterizerState == nullptr)
    {
        return;
    }

    const float ClearColor[4] = {0.05f, 0.05f, 0.07f, 1.0f};

    ID3D11ShaderResourceView* NullSRV = nullptr;
    mDeviceContext->PSSetShaderResources(0, 1, &NullSRV);

    mDeviceContext->ClearRenderTargetView(AssetPreviewRTV, ClearColor);
    mDeviceContext->ClearDepthStencilView(AssetPreviewDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    mDeviceContext->OMSetRenderTargets(1, &AssetPreviewRTV, AssetPreviewDSV);
    mDeviceContext->RSSetViewports(1, &AssetPreviewRenderInfo);
    mDeviceContext->RSSetState(mRasterizerState);
    mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mDeviceContext->OMSetDepthStencilState(ViewportDepthState, 0);
}

void FD3D11RHI::EndAssetPreviewRender()
{
    EndViewportRender();
}

void FD3D11RHI::ReleaseAssetPreviewRenderTarget()
{
    if (mDeviceContext != nullptr)
    {
        mDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

        ID3D11ShaderResourceView* NullSRV = nullptr;
        mDeviceContext->PSSetShaderResources(0, 1, &NullSRV);
    }

    if (AssetPreviewSRV != nullptr)
    {
        AssetPreviewSRV->Release();
        AssetPreviewSRV = nullptr;
    }

    if (AssetPreviewRTV != nullptr)
    {
        AssetPreviewRTV->Release();
        AssetPreviewRTV = nullptr;
    }

    if (AssetPreviewDSV != nullptr)
    {
        AssetPreviewDSV->Release();
        AssetPreviewDSV = nullptr;
    }

    if (AssetPreviewTexture != nullptr)
    {
        AssetPreviewTexture->Release();
        AssetPreviewTexture = nullptr;
    }

    if (AssetPreviewDepthTexture != nullptr)
    {
        AssetPreviewDepthTexture->Release();
        AssetPreviewDepthTexture = nullptr;
    }

    AssetPreviewWidth = 0;
    AssetPreviewHeight = 0;
    AssetPreviewRenderInfo = {};
}

void FD3D11RHI::BeginViewportRender()
{
    if (mDeviceContext == nullptr || ViewportRTV == nullptr || ViewportDSV == nullptr || ViewportDepthState == nullptr || mRasterizerState == nullptr)
    {
        return;
    }

    const float ClearColor[4] = {0.05f, 0.05f, 0.07f, 1.0f};

    mDeviceContext->ClearRenderTargetView(ViewportRTV, ClearColor);
    mDeviceContext->ClearDepthStencilView(ViewportDSV, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    mDeviceContext->OMSetRenderTargets(1, &ViewportRTV, ViewportDSV);
    mDeviceContext->RSSetViewports(1, &ViewportRenderInfo);
    mDeviceContext->RSSetState(bWireframeEnabled ? mWireframeRasterizerState : mRasterizerState);
    mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    mDeviceContext->OMSetDepthStencilState(ViewportDepthState, 0);
}

void FD3D11RHI::EndViewportRender()
{
    if (mDeviceContext == nullptr || mFrameBufferRTV == nullptr)
    {
        return;
    }

    mDeviceContext->OMSetRenderTargets(1, &mFrameBufferRTV, nullptr);
    mDeviceContext->RSSetViewports(1, &mViewportInfo);
    mDeviceContext->OMSetDepthStencilState(nullptr, 0);
}

void FD3D11RHI::ReleaseViewportRenderTarget()
{
    if (mDeviceContext != nullptr)
    {
        mDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

        ID3D11ShaderResourceView* NullSRV = nullptr;
        mDeviceContext->PSSetShaderResources(0, 1, &NullSRV);
    }

    if (ViewportSRV != nullptr)
    {
        ViewportSRV->Release();
        ViewportSRV = nullptr;
    }

    if (ViewportRTV != nullptr)
    {
        ViewportRTV->Release();
        ViewportRTV = nullptr;
    }

    if (ViewportDSV != nullptr)
    {
        ViewportDSV->Release();
        ViewportDSV = nullptr;
    }

    if (ViewportDepthState != nullptr)
    {
        ViewportDepthState->Release();
        ViewportDepthState = nullptr;
    }

    if (ViewportTexture != nullptr)
    {
        ViewportTexture->Release();
        ViewportTexture = nullptr;
    }

    if (ViewportDepthTexture != nullptr)
    {
        ViewportDepthTexture->Release();
        ViewportDepthTexture = nullptr;
    }

    if (ViewportOverlayDepthState != nullptr)
    {
        ViewportOverlayDepthState->Release();
        ViewportOverlayDepthState = nullptr;
    }

    ViewportWidth = 0;
    ViewportHeight = 0;
    ViewportRenderInfo = {};
}

bool FD3D11RHI::IsReady() const
{
    return !bInitializationFailed &&
        mDevice != nullptr &&
        mDeviceContext != nullptr &&
        mSwapChain != nullptr &&
        mFrameBufferRTV != nullptr &&
        mRasterizerState != nullptr &&
        mOutlineRasterizerState != nullptr &&
        mSimpleVertexShader != nullptr &&
        mSimplePixelShader != nullptr &&
        mSimpleInputLayout != nullptr &&
        mTextureSamplerState != nullptr &&
        mConstantBuffer != nullptr &&
        ViewportTexture != nullptr &&
        ViewportRTV != nullptr &&
        ViewportSRV != nullptr &&
        ViewportDepthTexture != nullptr &&
        ViewportDSV != nullptr &&
        ViewportDepthState != nullptr &&
        mWireframeRasterizerState != nullptr &&
        mAlphaBlendState != nullptr &&
        ViewportOverlayDepthState != nullptr;
}

void FD3D11RHI::Shutdown()
{
    ReleaseAssetPreviewRenderTarget();
    ReleaseViewportRenderTarget();
    ReleaseConstantBuffer();
    ReleaseShader();
    Release();
}

void FD3D11RHI::ReleaseDynamicVertexBuffer()
{
    if (mDynamicVertexBuffer != nullptr)
    {
        mDynamicVertexBuffer->Release();
        mDynamicVertexBuffer = nullptr;
    }

    mDynamicVertexCapacity = 0;
}

void FD3D11RHI::ReleaseDynamicIndexBuffer()
{
    if (mDynamicIndexBuffer != nullptr)
    {
        mDynamicIndexBuffer->Release();
        mDynamicIndexBuffer = nullptr;
    }

    mDynamicIndexCapacity = 0;
}

bool FD3D11RHI::DrawDynamicIndexedLines(const FVertexSimple* Vertices, UINT VertexCount, const uint32* Indices, UINT IndexCount, const FMatrix& View, const FMatrix& Projection)
{
    if (mDevice == nullptr || mDeviceContext == nullptr || Vertices == nullptr || Indices == nullptr || VertexCount == 0 || IndexCount == 0 || IndexCount % 2 != 0 || mSimpleVertexShader == nullptr || mSimplePixelShader == nullptr || mSimpleInputLayout == nullptr || mConstantBuffer == nullptr || mRasterizerState == nullptr || mAlphaBlendState == nullptr || ViewportDepthState == nullptr || ViewportOverlayDepthState == nullptr)
    {
        return false;
    }

    for (UINT Index = 0; Index < IndexCount; ++Index)
    {
        if (Indices[Index] >= VertexCount)
        {
            return false;
        }
    }

    const auto GrowCapacity = [](UINT Current, UINT Required, UINT& OutCapacity)
    {
        UINT Capacity = Current == 0 ? 256 : Current;

        while (Capacity < Required)
        {
            if (Capacity > (std::numeric_limits<UINT>::max)() / 2)
            {
                Capacity = Required;
                break;
            }

            Capacity *= 2;
        }

        OutCapacity = Capacity;
        return Capacity >= Required;
    };

    if (mDynamicVertexCapacity < VertexCount)
    {
        UINT NewCapacity = 0;

        if (!GrowCapacity(mDynamicVertexCapacity, VertexCount, NewCapacity))
        {
            return false;
        }

        const uint64 ByteWidth = sizeof(FVertexSimple) * static_cast<uint64>(NewCapacity);

        if (ByteWidth > (std::numeric_limits<UINT>::max)())
        {
            return false;
        }

        ReleaseDynamicVertexBuffer();

        D3D11_BUFFER_DESC BufferDesc = {};
        BufferDesc.ByteWidth = static_cast<UINT>(ByteWidth);
        BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (FAILED(mDevice->CreateBuffer(&BufferDesc, nullptr, &mDynamicVertexBuffer)) || mDynamicVertexBuffer == nullptr)
        {
            ReleaseDynamicVertexBuffer();
            return false;
        }

        mDynamicVertexCapacity = NewCapacity;
    }

    if (mDynamicIndexCapacity < IndexCount)
    {
        UINT NewCapacity = 0;

        if (!GrowCapacity(mDynamicIndexCapacity, IndexCount, NewCapacity))
        {
            return false;
        }

        const uint64 ByteWidth = sizeof(uint32) * static_cast<uint64>(NewCapacity);

        if (ByteWidth > (std::numeric_limits<UINT>::max)())
        {
            return false;
        }

        ReleaseDynamicIndexBuffer();

        D3D11_BUFFER_DESC BufferDesc = {};
        BufferDesc.ByteWidth = static_cast<UINT>(ByteWidth);
        BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        BufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        if (FAILED(mDevice->CreateBuffer(&BufferDesc, nullptr, &mDynamicIndexBuffer)) || mDynamicIndexBuffer == nullptr)
        {
            ReleaseDynamicIndexBuffer();
            return false;
        }

        mDynamicIndexCapacity = NewCapacity;
    }

    D3D11_MAPPED_SUBRESOURCE MappedVertices = {};

    if (FAILED(mDeviceContext->Map(mDynamicVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedVertices)) || MappedVertices.pData == nullptr)
    {
        return false;
    }

    std::memcpy(MappedVertices.pData, Vertices, sizeof(FVertexSimple) * static_cast<SIZE_T>(VertexCount));
    mDeviceContext->Unmap(mDynamicVertexBuffer, 0);

    D3D11_MAPPED_SUBRESOURCE MappedIndices = {};

    if (FAILED(mDeviceContext->Map(mDynamicIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedIndices)) || MappedIndices.pData == nullptr)
    {
        return false;
    }

    std::memcpy(MappedIndices.pData, Indices, sizeof(uint32) * static_cast<SIZE_T>(IndexCount));
    mDeviceContext->Unmap(mDynamicIndexBuffer, 0);

    UpdateConstant(FMatrix::Identity, View, Projection, false, false, true);

    mDeviceContext->VSSetShader(mSimpleVertexShader, nullptr, 0);
    mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
    mDeviceContext->PSSetShader(mSimplePixelShader, nullptr, 0);
    mDeviceContext->PSSetConstantBuffers(0, 1, &mConstantBuffer);
    mDeviceContext->IASetInputLayout(mSimpleInputLayout);

    const UINT Offset = 0;

    mDeviceContext->IASetVertexBuffers(0, 1, &mDynamicVertexBuffer, &mStride, &Offset);
    mDeviceContext->IASetIndexBuffer(mDynamicIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
    mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    ID3D11ShaderResourceView* NullSRV = nullptr;
    mDeviceContext->PSSetShaderResources(0, 1, &NullSRV);

    const FLOAT BlendFactor[4] = {};

    mDeviceContext->OMSetBlendState(mAlphaBlendState, BlendFactor, 0xffffffff);
    mDeviceContext->OMSetDepthStencilState(ViewportOverlayDepthState, 0);
    mDeviceContext->RSSetState(mRasterizerState);
    mDeviceContext->DrawIndexed(IndexCount, 0, 0);
    mDeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    mDeviceContext->OMSetDepthStencilState(ViewportDepthState, 0);
    mDeviceContext->RSSetState(bWireframeEnabled ? mWireframeRasterizerState : mRasterizerState);
    mDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}

bool FD3D11RHI::DrawDynamicVerticesNative(const FVertexSimple* Vertices, UINT VertexCount, D3D11_PRIMITIVE_TOPOLOGY Topology, const FMatrix& World, const FMatrix& View, const FMatrix& Projection, ID3D11ShaderResourceView* TextureSRV, bool bIsTextured, bool bAlphaBlend)
{
    if (mDevice == nullptr || mDeviceContext == nullptr || Vertices == nullptr || VertexCount == 0 || mSimpleVertexShader == nullptr || mSimplePixelShader == nullptr || mSimpleInputLayout == nullptr || (bIsTextured && TextureSRV == nullptr) || mRasterizerState == nullptr || mAlphaBlendState == nullptr || ViewportDepthState == nullptr || ViewportOverlayDepthState == nullptr)
    {
        return false;
    }

    if (Topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST && Topology != D3D11_PRIMITIVE_TOPOLOGY_LINELIST)
    {
        return false;
    }

    if ((Topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST && VertexCount % 3 != 0) || (Topology == D3D11_PRIMITIVE_TOPOLOGY_LINELIST && VertexCount % 2 != 0))
    {
        return false;
    }

    if (mDynamicVertexCapacity < VertexCount)
    {
        UINT NewCapacity = 256;

        while (NewCapacity < VertexCount)
        {
            if (NewCapacity > (std::numeric_limits<UINT>::max)() / 2)
            {
                NewCapacity = VertexCount;
                break;
            }

            NewCapacity *= 2;
        }

        const uint64 ByteWidth = sizeof(FVertexSimple) * static_cast<uint64>(NewCapacity);

        if (ByteWidth > (std::numeric_limits<UINT>::max)())
        {
            return false;
        }

        ReleaseDynamicVertexBuffer();

        D3D11_BUFFER_DESC BufferDesc = {};
        BufferDesc.ByteWidth = static_cast<UINT>(ByteWidth);
        BufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        BufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        BufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        const HRESULT CreateResult = mDevice->CreateBuffer(&BufferDesc, nullptr, &mDynamicVertexBuffer);

        if (FAILED(CreateResult) || mDynamicVertexBuffer == nullptr)
        {
            ReleaseDynamicVertexBuffer();
            return false;
        }

        mDynamicVertexCapacity = NewCapacity;
    }

    D3D11_MAPPED_SUBRESOURCE MappedResource = {};

    const HRESULT MapResult = mDeviceContext->Map(mDynamicVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);

    if (FAILED(MapResult) || MappedResource.pData == nullptr)
    {
        return false;
    }

    std::memcpy(MappedResource.pData, Vertices, sizeof(FVertexSimple) * static_cast<SIZE_T>(VertexCount));
    mDeviceContext->Unmap(mDynamicVertexBuffer, 0);

    UpdateConstant(World, View, Projection, false, bIsTextured, true);

    mDeviceContext->VSSetShader(mSimpleVertexShader, nullptr, 0);
    mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
    mDeviceContext->PSSetShader(mSimplePixelShader, nullptr, 0);
    mDeviceContext->PSSetConstantBuffers(0, 1, &mConstantBuffer);
    mDeviceContext->IASetInputLayout(mSimpleInputLayout);

    UINT Offset = 0;

    mDeviceContext->IASetVertexBuffers(0, 1, &mDynamicVertexBuffer, &mStride, &Offset);
    mDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    mDeviceContext->IASetPrimitiveTopology(Topology);

    ID3D11ShaderResourceView* SRV = bIsTextured ? TextureSRV : nullptr;
    mDeviceContext->PSSetShaderResources(0, 1, &SRV);

    if (bIsTextured)
    {
        mDeviceContext->PSSetSamplers(0, 1, &mTextureSamplerState);
    }

    const FLOAT BlendFactor[4] = {};

    mDeviceContext->OMSetBlendState(bAlphaBlend ? mAlphaBlendState : nullptr, BlendFactor, 0xffffffff);
    mDeviceContext->OMSetDepthStencilState(ViewportOverlayDepthState, 0);
    mDeviceContext->RSSetState(mRasterizerState);
    mDeviceContext->Draw(VertexCount, 0);

    ID3D11ShaderResourceView* NullSRV = nullptr;

    mDeviceContext->PSSetShaderResources(0, 1, &NullSRV);
    mDeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    mDeviceContext->OMSetDepthStencilState(ViewportDepthState, 0);
    mDeviceContext->RSSetState(bWireframeEnabled ? mWireframeRasterizerState : mRasterizerState);
    mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    return true;
}
// ----------------------------------------------------------------------------
// FRHI interface: convert backend-neutral handles to D3D11 and forward
// ----------------------------------------------------------------------------

void FD3D11RHI::RenderPrimitive(FRHIBuffer* VertexBuffer, FRHIBuffer* IndexBuffer, uint32 IndexCount, bool bRenderOutline, FRHIShaderResourceView* TextureSRV, uint32 StartIndexLocation)
{
    RenderPrimitiveNative(ToD3D(VertexBuffer), ToD3D(IndexBuffer), IndexCount, bRenderOutline, ToD3D(TextureSRV), StartIndexLocation);
}

bool FD3D11RHI::DrawDynamicVertices(const FVertexSimple* Vertices, uint32 VertexCount, ERHIPrimitiveTopology Topology, const FMatrix& World, const FMatrix& View, const FMatrix& Projection, FRHIShaderResourceView* TextureSRV, bool bIsTextured, bool bAlphaBlend)
{
    const D3D11_PRIMITIVE_TOPOLOGY NativeTopology = Topology == ERHIPrimitiveTopology::LineList ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    return DrawDynamicVerticesNative(Vertices, VertexCount, NativeTopology, World, View, Projection, ToD3D(TextureSRV), bIsTextured, bAlphaBlend);
}

FRHIBuffer* FD3D11RHI::CreateVertexBuffer(FVertexSimple* Vertices, uint32 ByteWidth)
{
    return reinterpret_cast<FRHIBuffer*>(CreateVertexBufferNative(Vertices, ByteWidth));
}

void FD3D11RHI::ReleaseVertexBuffer(FRHIBuffer* VertexBuffer)
{
    ReleaseVertexBufferNative(ToD3D(VertexBuffer));
}

FRHIBuffer* FD3D11RHI::CreateIndexBuffer(const uint32* Indices, uint32 IndexCount)
{
    return reinterpret_cast<FRHIBuffer*>(CreateIndexBufferNative(Indices, IndexCount));
}

void FD3D11RHI::ReleaseIndexBuffer(FRHIBuffer* IndexBuffer)
{
    ReleaseIndexBufferNative(ToD3D(IndexBuffer));
}

void FD3D11RHI::ReleaseTexture2D(FTexture2DResource& Resource)
{
    if (ID3D11ShaderResourceView* View = ToD3D(Resource.ShaderResourceView))
    {
        View->Release();
    }

    if (ID3D11Texture2D* Texture = ToD3D(Resource.Texture))
    {
        Texture->Release();
    }

    Resource.ShaderResourceView = nullptr;
    Resource.Texture = nullptr;
}

bool FD3D11RHI::CopyAssetPreviewToTexture(FTexture2DResource& OutResource)
{
    if (mDevice == nullptr || mDeviceContext == nullptr || AssetPreviewTexture == nullptr || OutResource.IsValid())
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC TextureDesc = {};
    AssetPreviewTexture->GetDesc(&TextureDesc);
    TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.MiscFlags = 0;
    TextureDesc.CPUAccessFlags = 0;
    TextureDesc.Usage = D3D11_USAGE_DEFAULT;

    ID3D11Texture2D* NewTexture = nullptr;

    if (FAILED(mDevice->CreateTexture2D(&TextureDesc, nullptr, &NewTexture)) || NewTexture == nullptr)
    {
        return false;
    }

    mDeviceContext->CopyResource(NewTexture, AssetPreviewTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC ViewDesc = {};
    ViewDesc.Format = TextureDesc.Format;
    ViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    ViewDesc.Texture2D.MipLevels = TextureDesc.MipLevels;

    ID3D11ShaderResourceView* NewView = nullptr;

    if (FAILED(mDevice->CreateShaderResourceView(NewTexture, &ViewDesc, &NewView)) || NewView == nullptr)
    {
        NewTexture->Release();
        return false;
    }

    OutResource.Texture = reinterpret_cast<FRHITexture*>(NewTexture);
    OutResource.ShaderResourceView = reinterpret_cast<FRHIShaderResourceView*>(NewView);
    OutResource.Width = TextureDesc.Width;
    OutResource.Height = TextureDesc.Height;
    return true;
}

// ----------------------------------------------------------------------------
// Backend creation
// ----------------------------------------------------------------------------

FRHI* RHICreate()
{
    if (GRHI == nullptr)
    {
        GRHI = new FD3D11RHI();
    }

    return GRHI;
}

void RHIDestroy()
{
    delete GRHI;
    GRHI = nullptr;
}
