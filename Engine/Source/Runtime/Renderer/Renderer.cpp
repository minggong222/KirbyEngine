
#include <d3d11.h>
#include <d3dcompiler.h>

#include "Engine/Source/Runtime/Renderer/Renderer.h"
#include "Resources/TextureResource.h"

#include "Core/Math/Math.h"

#include <cstring>
#include <limits>

// 렌더러 초기화 함수
void URenderer::Create(HWND hWindow)
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
void URenderer::CreateDeviceAndSwapChain(HWND hWindow)
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
void URenderer::ReleaseDeviceAndSwapChain()
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
void URenderer::CreateFrameBuffer()
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
void URenderer::ReleaseFrameBuffer()
{
    if (mFrameBufferRTV)
    {
        mFrameBufferRTV->Release();
        mFrameBufferRTV = nullptr;
    }
    
    if (mFrameBuffer)
    {
        mFrameBuffer->Release();
        mFrameBuffer = nullptr;
    }
}

// 래스터라이저 상태를 생성하는 함수
void URenderer::CreateRasterizerState()
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
void URenderer::ReleaseRasterizerState()
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

    if (mOutlineRasterizerState)
    {
        mOutlineRasterizerState->Release();
        mOutlineRasterizerState = nullptr;
    }

    if (mRasterizerState)
    {
        mRasterizerState->Release();
        mRasterizerState = nullptr;
    }
}

// 렌더러에 사용된 모든 리소스를 해제하는 함수
void URenderer::Release()
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
void URenderer::SwapBuffer()
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

void URenderer::CreateShader()
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
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0}
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

void URenderer::ReleaseShader()
{
    if (mSimpleInputLayout)
    {
        mSimpleInputLayout->Release();
        mSimpleInputLayout = nullptr;
    }

    if (mSimplePixelShader)
    {
        mSimplePixelShader->Release();
        mSimplePixelShader = nullptr;
    }

    if (mSimpleVertexShader)
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

void URenderer::Prepare()
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

void URenderer::PrepareShader()
{
    if (mDeviceContext == nullptr || mSimpleVertexShader == nullptr || mSimplePixelShader == nullptr || mSimpleInputLayout == nullptr)
    {
        return;
    }
    
    mDeviceContext->VSSetShader(mSimpleVertexShader, nullptr, 0);
    mDeviceContext->PSSetShader(mSimplePixelShader, nullptr, 0);
    mDeviceContext->IASetInputLayout(mSimpleInputLayout);

    // 버텍스 쉐이더에 상수 버퍼를 설정합니다.
    if (mConstantBuffer)
    {
        mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
    }
}

void URenderer::RenderPrimitive(ID3D11Buffer* VertexBuffer, ID3D11Buffer* IndexBuffer, UINT IndexCount, bool bRenderOutline, ID3D11ShaderResourceView* TextureSRV)
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

    mDeviceContext->DrawIndexed(IndexCount, 0, 0);

    if (bRenderOutline)
    {
        mDeviceContext->RSSetState(bWireframeEnabled ? mWireframeRasterizerState : mRasterizerState);
    }
}

void URenderer::SetViewMode(EViewMode InViewMode)
{
    bWireframeEnabled = InViewMode == EViewMode::Wireframe;
    bUnlitEnabled = InViewMode == EViewMode::Unlit;

    if (mDeviceContext != nullptr)
    {
        mDeviceContext->RSSetState(bWireframeEnabled ? mWireframeRasterizerState : mRasterizerState);
    }
}

ID3D11Buffer* URenderer::CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
{
    if (mDevice == nullptr || vertices == nullptr || byteWidth == 0)
    {
        return nullptr;
    }
    // 2. Create a vertex buffer
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated 
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

void URenderer::ReleaseVertexBuffer(ID3D11Buffer* VertexBuffer)
{
    if (VertexBuffer != nullptr)
    {
        VertexBuffer->Release();
    }
}

ID3D11Buffer* URenderer::CreateIndexBuffer(const uint32* Indices, UINT IndexCount)
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

void URenderer::ReleaseIndexBuffer(ID3D11Buffer* IndexBuffer)
{
    if (IndexBuffer != nullptr)
    {
        IndexBuffer->Release();
    }
}

bool URenderer::CreateTexture2D(const uint8* Pixels, uint32 Width, uint32 Height, FTexture2DResource& OutResource)
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

    OutResource.Texture = NewTexture;
    OutResource.ShaderResourceView = NewShaderResourceView;
    OutResource.Width = Width;
    OutResource.Height = Height;

    return true;
}

void URenderer::CreateConstantBuffer()
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

void URenderer::ReleaseConstantBuffer()
{
    if (mConstantBuffer)
    {
        mConstantBuffer->Release();
        mConstantBuffer = nullptr;
    }
}

void URenderer::UpdateConstant(FMatrix World, FMatrix View, FMatrix Projection, bool bIsOutline, bool bIsTextured, bool bForceUnlit)
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
    Constants->IsOutline = bIsOutline ? 1u : 0u;
    Constants->IsTextured = bIsTextured ? 1u : 0u;
    Constants->IsUnlit = (bUnlitEnabled || bForceUnlit) ? 1u : 0u;

    mDeviceContext->Unmap(mConstantBuffer, 0);
}

ID3D11Device* URenderer::GetDevice()
{
    return mDevice;
}

ID3D11DeviceContext* URenderer::GetDeviceContext()
{
    return mDeviceContext;
}

void URenderer::ResizeBackBuffer(uint32 Width, uint32 Height)
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

void URenderer::CreateViewportRenderTarget(uint32 Width, uint32 Height)
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

void URenderer::ResizeViewportRenderTarget(uint32 Width, uint32 Height)
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

void URenderer::BeginViewportRender()
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

void URenderer::EndViewportRender()
{
    if (mDeviceContext == nullptr || mFrameBufferRTV == nullptr)
    {
        return;
    }

    mDeviceContext->OMSetRenderTargets(1, &mFrameBufferRTV, nullptr);
    mDeviceContext->RSSetViewports(1, &mViewportInfo);
    mDeviceContext->OMSetDepthStencilState(nullptr, 0);
}

void URenderer::ReleaseViewportRenderTarget()
{
    if (mDeviceContext)
    {
        mDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

        ID3D11ShaderResourceView* NullSRV = nullptr;
        mDeviceContext->PSSetShaderResources(0, 1, &NullSRV);
    }

    if (ViewportSRV)
    {
        ViewportSRV->Release();
        ViewportSRV = nullptr;
    }

    if (ViewportRTV)
    {
        ViewportRTV->Release();
        ViewportRTV = nullptr;
    }

    if (ViewportDSV)
    {
        ViewportDSV->Release();
        ViewportDSV = nullptr;
    }

    if (ViewportDepthState)
    {
        ViewportDepthState->Release();
        ViewportDepthState = nullptr;
    }

    if (ViewportTexture)
    {
        ViewportTexture->Release();
        ViewportTexture = nullptr;
    }

    if (ViewportDepthTexture)
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

bool URenderer::IsReady() const
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

void URenderer::Shutdown()
{
    ReleaseViewportRenderTarget();
    //ReleaseRasterizerState();
    ReleaseConstantBuffer();
    ReleaseShader();
    Release();
}


void URenderer::ReleaseDynamicVertexBuffer()
{
    if (mDynamicVertexBuffer != nullptr)
    {
        mDynamicVertexBuffer->Release();
        mDynamicVertexBuffer = nullptr;
    }

    mDynamicVertexCapacity = 0;
}

void URenderer::ReleaseDynamicIndexBuffer()
{
    if (mDynamicIndexBuffer != nullptr)
    {
        mDynamicIndexBuffer->Release();
        mDynamicIndexBuffer = nullptr;
    }

    mDynamicIndexCapacity = 0;
}

bool URenderer::DrawDynamicIndexedLines(const FVertexSimple* Vertices, UINT VertexCount, const uint32* Indices, UINT IndexCount, const FMatrix& View, const FMatrix& Projection)
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
        if (!GrowCapacity(mDynamicVertexCapacity, VertexCount, NewCapacity)) return false;
        const uint64 ByteWidth = sizeof(FVertexSimple) * static_cast<uint64>(NewCapacity);
        if (ByteWidth > (std::numeric_limits<UINT>::max)()) return false;
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
        if (!GrowCapacity(mDynamicIndexCapacity, IndexCount, NewCapacity)) return false;
        const uint64 ByteWidth = sizeof(uint32) * static_cast<uint64>(NewCapacity);
        if (ByteWidth > (std::numeric_limits<UINT>::max)()) return false;
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
    if (FAILED(mDeviceContext->Map(mDynamicVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedVertices)) || MappedVertices.pData == nullptr) return false;
    std::memcpy(MappedVertices.pData, Vertices, sizeof(FVertexSimple) * static_cast<SIZE_T>(VertexCount));
    mDeviceContext->Unmap(mDynamicVertexBuffer, 0);

    D3D11_MAPPED_SUBRESOURCE MappedIndices = {};
    if (FAILED(mDeviceContext->Map(mDynamicIndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedIndices)) || MappedIndices.pData == nullptr) return false;
    std::memcpy(MappedIndices.pData, Indices, sizeof(uint32) * static_cast<SIZE_T>(IndexCount));
    mDeviceContext->Unmap(mDynamicIndexBuffer, 0);

    UpdateConstant(FMatrix::Identity, View, Projection, false, false, true);
    mDeviceContext->VSSetShader(mSimpleVertexShader, nullptr, 0);
    mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
    mDeviceContext->PSSetShader(mSimplePixelShader, nullptr, 0);
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

bool URenderer::DrawDynamicVertices(const FVertexSimple* Vertices, UINT VertexCount, D3D11_PRIMITIVE_TOPOLOGY Topology, const FMatrix& World, const FMatrix& View, const FMatrix& Projection, ID3D11ShaderResourceView* TextureSRV, bool bIsTextured, bool bAlphaBlend)
{
    if (mDevice == nullptr || mDeviceContext == nullptr || Vertices == nullptr || VertexCount == 0 || mSimpleVertexShader == nullptr || mSimplePixelShader == nullptr || mSimpleInputLayout == nullptr || (bIsTextured && TextureSRV == nullptr) ||
        mRasterizerState == nullptr || mAlphaBlendState == nullptr || ViewportDepthState == nullptr || ViewportOverlayDepthState == nullptr)
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
    if (mConstantBuffer != nullptr)
    {
        mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
    }
    mDeviceContext->PSSetShader(mSimplePixelShader, nullptr, 0);
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
