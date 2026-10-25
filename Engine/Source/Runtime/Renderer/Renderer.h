#pragma once
#include <d3d11.h>
#include "FVertexSimple.h"
#include "Core/STL.h"
#include "Core/Math/Math.h"

struct FConstants
{
    FMatrix World;
    FMatrix View;
    FMatrix Projection;
};

class URenderer
{
public:
    void Create(HWND hWindow);
    void CreateDeviceAndSwapChain(HWND hWindow);
    void ReleaseDeviceAndSwapChain();
    void CreateFrameBuffer();
    void ReleaseFrameBuffer();
    void CreateRasterizerState();
    void ReleaseRasterizerState();
    void Release();
    void SwapBuffer();

    void CreateShader();
    void ReleaseShader();

    void Prepare();
    void PrepareShader();
    void RenderPrimitive(ID3D11Buffer* pBuffer, UINT numVertices);

    ID3D11Buffer* CreateVertexBuffer(FVertexSimple* vertices, UINT byteWidth);
    void ReleaseVertexBuffer(ID3D11Buffer* vertexBuffer);

    void CreateConstantBuffer();
    void ReleaseConstantBuffer();
    void UpdateConstant(FMatrix World, FMatrix View, FMatrix Projection);

    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetDeviceContext();

    ID3D11ShaderResourceView* GetViewportSRV() const{ return ViewportSRV; }
    
    // Viewport Render
    void CreateViewportRenderTarget(uint32 Width, uint32 Height);
    void BeginViewportRender();
    void EndViewportRender();
    void ReleaseViewportRenderTarget();
    
    void Shutdown();
private:
    // Direct3D 11 장치(Device)와 장치 컨텍스트(Device Context) 및 스왑 체인(Swap Chain)을 관리하기 위한 포인터들
    ID3D11Device* mDevice = nullptr; // GPU와 통신하기 위한 Direct3D 장치
    ID3D11DeviceContext* mDeviceContext = nullptr; // GPU 명령 실행을 담당하는 컨텍스트
    IDXGISwapChain* mSwapChain = nullptr; // 프레임 버퍼를 교체하는 데 사용되는 스왑 체인

    // 렌더링에 필요한 리소스 및 상태를 관리하기 위한 변수들
    ID3D11Texture2D* mFrameBuffer = nullptr; // 화면 출력용 텍스처
    ID3D11RenderTargetView* mFrameBufferRTV = nullptr; // 텍스처를 렌더 타겟으로 사용하는 뷰
    ID3D11RasterizerState* mRasterizerState = nullptr; // 래스터라이저 상태(컬링, 채우기 모드 등 정의)
    ID3D11Buffer* mConstantBuffer = nullptr; // 쉐이더에 데이터를 전달하기 위한 상수 버퍼

    FLOAT mClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f }; // 화면을 초기화(clear)할 때 사용할 색상 (RGBA)
    D3D11_VIEWPORT mViewportInfo; // 렌더링 영역을 정의하는 뷰포트 정보

    ID3D11VertexShader* mSimpleVertexShader;
    ID3D11PixelShader* mSimplePixelShader;
    ID3D11InputLayout* mSimpleInputLayout;
    unsigned int mStride;
    
    ID3D11Texture2D* ViewportTexture = nullptr;
    ID3D11RenderTargetView* ViewportRTV = nullptr;
    ID3D11ShaderResourceView* ViewportSRV = nullptr;
    ID3D11Texture2D* ViewportDepthTexture = nullptr;
    ID3D11DepthStencilView* ViewportDSV = nullptr;
    D3D11_VIEWPORT ViewportRenderInfo = {};
    ID3D11DepthStencilState* ViewportDepthState = nullptr;
};
