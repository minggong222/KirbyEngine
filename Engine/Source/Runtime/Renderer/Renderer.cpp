
#include <d3d11.h>
#include <d3dcompiler.h>

#include "Engine/Source/Runtime/Renderer/Renderer.h"

#include "Core/Math/Math.h"

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
    }
}

// 래스터라이저 상태를 해제하는 함수
void URenderer::ReleaseRasterizerState()
{
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

    HRESULT hr = D3DCompileFromFile(L"Engine/Resources/Shaders/ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &VertexShaderBlob, &ErrorBlob);

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

    hr = D3DCompileFromFile(L"Engine/Resources/Shaders/ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &PixelShaderBlob, &ErrorBlob);

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
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
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
    mDeviceContext->RSSetState(mRasterizerState);

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

void URenderer::RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices, bool bRenderOutline)
{
    if (mDeviceContext == nullptr || pBuffer == nullptr || mRasterizerState == nullptr || (bRenderOutline && mOutlineRasterizerState == nullptr))
    {
        return;
    }
    
    if (bRenderOutline)
    {
        mDeviceContext->RSSetState(mOutlineRasterizerState);
    }

    UINT offset = 0;
    mDeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &mStride, &offset);

    mDeviceContext->Draw(numVertices, 0);

    if (bRenderOutline)
    {
        mDeviceContext->RSSetState(mRasterizerState);
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

void URenderer::ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
{
    vertexBuffer->Release();
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

void URenderer::UpdateConstant(FMatrix World, FMatrix View, FMatrix Projection, bool bIsOutline)
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
    mDeviceContext->RSSetState(mRasterizerState);
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
        mConstantBuffer != nullptr &&
        ViewportTexture != nullptr &&
        ViewportRTV != nullptr &&
        ViewportSRV != nullptr &&
        ViewportDepthTexture != nullptr &&
        ViewportDSV != nullptr &&
        ViewportDepthState != nullptr;
}

void URenderer::Shutdown()
{
    ReleaseViewportRenderTarget();
    //ReleaseRasterizerState();
    ReleaseConstantBuffer();
    ReleaseShader();
    Release();
}
