
#include <d3d11.h>
#include <d3dcompiler.h>

#include "Engine/Source/Runtime/Renderer/Renderer.h"

#include "Core/Math/Math.h"

// 렌더러 초기화 함수
void URenderer::Create(HWND hWindow)
{
    // Direct3D 장치 및 스왑 체인 생성
    CreateDeviceAndSwapChain(hWindow);

    // 프레임 버퍼 생성
    CreateFrameBuffer();

    // 래스터라이저 상태 생성
    CreateRasterizerState();
}

// Direct3D 장치 및 스왑 체인을 생성하는 함수
void URenderer::CreateDeviceAndSwapChain(HWND hWindow)
{
    // 지원하는 Direct3D 기능 레벨을 정의
    D3D_FEATURE_LEVEL featurelevels[] = { D3D_FEATURE_LEVEL_11_0 };

    // 스왑 체인 설정 구조체 초기화
    DXGI_SWAP_CHAIN_DESC swapchaindesc = {};
    swapchaindesc.BufferDesc.Width = 0; // 창 크기에 맞게 자동으로 설정
    swapchaindesc.BufferDesc.Height = 0; // 창 크기에 맞게 자동으로 설정
    swapchaindesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 색상 포맷
    swapchaindesc.SampleDesc.Count = 1; // 멀티 샘플링 비활성화
    swapchaindesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 렌더 타겟으로 사용
    swapchaindesc.BufferCount = 2; // 더블 버퍼링
    swapchaindesc.OutputWindow = hWindow; // 렌더링할 창 핸들
    swapchaindesc.Windowed = TRUE; // 창 모드
    swapchaindesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // 스왑 방식

    // Direct3D 장치와 스왑 체인을 생성
    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
        featurelevels, ARRAYSIZE(featurelevels), D3D11_SDK_VERSION,
        &swapchaindesc, &mSwapChain, &mDevice, nullptr, &mDeviceContext);

    // 생성된 스왑 체인의 정보 가져오기
    mSwapChain->GetDesc(&swapchaindesc);

    // 뷰포트 정보 설정
    mViewportInfo = { 0.0f, 0.0f, (float)swapchaindesc.BufferDesc.Width, (float)swapchaindesc.BufferDesc.Height, 0.0f, 1.0f };
}

// Direct3D 장치 및 스왑 체인을 해제하는 함수
void URenderer::ReleaseDeviceAndSwapChain()
{
    if (mDeviceContext)
    {
        mDeviceContext->Flush(); // 남아있는 GPU 명령 실행
    }

    if (mSwapChain)
    {
        mSwapChain->Release();
        mSwapChain = nullptr;
    }

    if (mDevice)
    {
        mDevice->Release();
        mDevice = nullptr;
    }

    if (mDeviceContext)
    {
        mDeviceContext->Release();
        mDeviceContext = nullptr;
    }
}

// 프레임 버퍼를 생성하는 함수
void URenderer::CreateFrameBuffer()
{
    // 스왑 체인으로부터 백 버퍼 텍스처 가져오기
    mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&mFrameBuffer);

    // 렌더 타겟 뷰 생성
    D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
    framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; // 색상 포맷
    framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D; // 2D 텍스처

    mDevice->CreateRenderTargetView(mFrameBuffer, &framebufferRTVdesc, &mFrameBufferRTV);
}

// 프레임 버퍼를 해제하는 함수
void URenderer::ReleaseFrameBuffer()
{
    if (mFrameBuffer)
    {
        mFrameBuffer->Release();
        mFrameBuffer = nullptr;
    }

    if (mFrameBufferRTV)
    {
        mFrameBufferRTV->Release();
        mFrameBufferRTV = nullptr;
    }
}

// 래스터라이저 상태를 생성하는 함수
void URenderer::CreateRasterizerState()
{
    D3D11_RASTERIZER_DESC rasterizerdesc = {};
    rasterizerdesc.FillMode = D3D11_FILL_SOLID; // 채우기 모드
    rasterizerdesc.CullMode = D3D11_CULL_BACK; // 백 페이스 컬링

    mDevice->CreateRasterizerState(&rasterizerdesc, &mRasterizerState);
}

// 래스터라이저 상태를 해제하는 함수
void URenderer::ReleaseRasterizerState()
{
    if (mRasterizerState)
    {
        mRasterizerState->Release();
        mRasterizerState = nullptr;
    }
}

// 렌더러에 사용된 모든 리소스를 해제하는 함수
void URenderer::Release()
{
    mRasterizerState->Release();

    // 렌더 타겟을 초기화
    mDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

    ReleaseFrameBuffer();
    ReleaseDeviceAndSwapChain();
}

// 스왑 체인의 백 버퍼와 프론트 버퍼를 교체하여 화면에 출력
void URenderer::SwapBuffer()
{
    mSwapChain->Present(0, 0);
}

void URenderer::CreateShader()
{
    ID3DBlob* vertexshaderCSO;
    ID3DBlob* pixelshaderCSO;

    D3DCompileFromFile(L"Engine/Resources/Shaders/ShaderW0.hlsl", nullptr, nullptr, "mainVS", "vs_5_0", 0, 0, &vertexshaderCSO, nullptr);
    mDevice->CreateVertexShader(vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), nullptr, &mSimpleVertexShader);

    D3DCompileFromFile(L"Engine/Resources/Shaders/ShaderW0.hlsl", nullptr, nullptr, "mainPS", "ps_5_0", 0, 0, &pixelshaderCSO, nullptr);
    mDevice->CreatePixelShader(pixelshaderCSO->GetBufferPointer(), pixelshaderCSO->GetBufferSize(), nullptr, &mSimplePixelShader);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    mDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vertexshaderCSO->GetBufferPointer(), vertexshaderCSO->GetBufferSize(), &mSimpleInputLayout);

    mStride = sizeof(FVertexSimple);

    vertexshaderCSO->Release();
    pixelshaderCSO->Release();
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
    mDeviceContext->ClearRenderTargetView(mFrameBufferRTV, mClearColor);
    mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    mDeviceContext->RSSetViewports(1, &mViewportInfo);
    mDeviceContext->RSSetState(mRasterizerState);

    mDeviceContext->OMSetRenderTargets(1, &mFrameBufferRTV, nullptr);
    mDeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
}

void URenderer::PrepareShader()
{
    mDeviceContext->VSSetShader(mSimpleVertexShader, nullptr, 0);
    mDeviceContext->PSSetShader(mSimplePixelShader, nullptr, 0);
    mDeviceContext->IASetInputLayout(mSimpleInputLayout);

    // 버텍스 쉐이더에 상수 버퍼를 설정합니다.
    if (mConstantBuffer)
    {
        mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
    }
}

void URenderer::RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices)
{
    UINT offset = 0;
    mDeviceContext->IASetVertexBuffers(0, 1, &pBuffer, &mStride, &offset);

    mDeviceContext->Draw(numVertices, 0);
}

ID3D11Buffer* URenderer::CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth)
{
    // 2. Create a vertex buffer
    D3D11_BUFFER_DESC vertexbufferdesc = {};
    vertexbufferdesc.ByteWidth = byteWidth;
    vertexbufferdesc.Usage = D3D11_USAGE_IMMUTABLE; // will never be updated 
    vertexbufferdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexbufferSRD = { vertices };

    ID3D11Buffer* vertexBuffer;
    mDevice->CreateBuffer(&vertexbufferdesc, &vertexbufferSRD, &vertexBuffer);

    return vertexBuffer;
}

void URenderer::ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer)
{
    vertexBuffer->Release();
}

void URenderer::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC constantbufferdesc = {};
    constantbufferdesc.ByteWidth = sizeof(FConstants) + 0xf & 0xfffffff0; // ensure constant buffer size is multiple of 16 bytes
    constantbufferdesc.Usage = D3D11_USAGE_DYNAMIC; // will be updated from CPU every frame
    constantbufferdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    constantbufferdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    mDevice->CreateBuffer(&constantbufferdesc, nullptr, &mConstantBuffer);
}

void URenderer::ReleaseConstantBuffer()
{
    if (mConstantBuffer)
    {
        mConstantBuffer->Release();
        mConstantBuffer = nullptr;
    }
}

void URenderer::UpdateConstant(FMatrix World, FMatrix View, FMatrix Projection)
{
    if (mConstantBuffer)
    {
        D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

        // update constant buffer every frame
        mDeviceContext->Map(mConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR); 
        FConstants* constants = (FConstants*)constantbufferMSR.pData;
        {
            constants->World = World;
            constants->View = View;
            constants->Projection = Projection;
        }

        mDeviceContext->Unmap(mConstantBuffer, 0);
    }
}

ID3D11Device* URenderer::GetDevice()
{
    return mDevice;
}

ID3D11DeviceContext* URenderer::GetDeviceContext()
{
    return mDeviceContext;
}

void URenderer::CreateViewportRenderTarget(uint32 Width, uint32 Height)
{
    assert(Width > 0 && Height > 0);
    
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
    assert(SUCCEEDED(hr));
    (void)hr;
    
    hr = mDevice->CreateRenderTargetView(ViewportTexture, nullptr, &ViewportRTV);
    assert(SUCCEEDED(hr));
    (void)hr;
    
    hr = mDevice->CreateShaderResourceView(ViewportTexture, nullptr, &ViewportSRV);
    assert(SUCCEEDED(hr));
    (void)hr;
    
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
    assert(SUCCEEDED(hr));
    (void)hr;
    
    hr = mDevice->CreateDepthStencilView(ViewportDepthTexture, nullptr, &ViewportDSV);
    assert(SUCCEEDED(hr));
    (void)hr;
    
    D3D11_DEPTH_STENCIL_DESC StateDesc = {};
    StateDesc.DepthEnable = TRUE;   
    StateDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    StateDesc.DepthFunc = D3D11_COMPARISON_LESS;
    hr = mDevice->CreateDepthStencilState(&StateDesc, &ViewportDepthState);
    assert(SUCCEEDED(hr));
    (void)hr;
    
    ViewportRenderInfo = {0.0f,0.0f,static_cast<float>(Width),static_cast<float>(Height), 0.0f,1.0f};
}

void URenderer::BeginViewportRender()
{
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
    mDeviceContext->OMSetRenderTargets(1, &mFrameBufferRTV, nullptr);
    mDeviceContext->RSSetViewports(1, &mViewportInfo);
    mDeviceContext->OMSetDepthStencilState(nullptr, 0);
}

void URenderer::ReleaseViewportRenderTarget()
{
    if (ViewportDepthState)
    {
        ViewportDepthState->Release();
        ViewportDepthState = nullptr;
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
}

void URenderer::Shutdown()
{
    ReleaseViewportRenderTarget();
    //ReleaseRasterizerState();
    ReleaseConstantBuffer();
    ReleaseShader();
    Release();
}
