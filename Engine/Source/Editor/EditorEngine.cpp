#include "EditorEngine.h"

#include "EditorCameraController.h"
#include "EditorObjectPicker.h"
#include "Components/CameraComponent.h"
#include "Components/SphereComp.h"
#include "Components/CubeComp.h"
#include "Core/STL.h"
#include "Core/Math/MathUtility.h"
#include "Core/Time/PlatformTime.h"
#include "UI/Panels/DetailsPanel.h"
#include "UI/Panels/ObjectListPanel.h"
#include "UObject/Object.h"

namespace 
{
    FStaticMeshData* CreateStaticMeshData(TArray<FVertexSimple>&& SourceVertices, URenderer& InRenderer)
    {
        if (SourceVertices.Num() < 3 || SourceVertices.Num() % 3 != 0)
        {
            return nullptr;
        }
        
        FStaticMeshData* MeshData = new FStaticMeshData();
        MeshData->Positions.Reserve(SourceVertices.Num());
        for (const FVertexSimple& Vertex : SourceVertices)
        {
            const FVector Position(Vertex.x, Vertex.y, Vertex.z);
            MeshData->Positions.Add(Position);
            MeshData->LocalBounds.Add(Position);
        }
        MeshData->VertexCount = SourceVertices.Num();
        const UINT ByteWidth = static_cast<UINT>(sizeof(FVertexSimple) * SourceVertices.Num());
        MeshData->VertexBuffer = InRenderer.CreateVertexBuffer(SourceVertices.GetData(), ByteWidth);
        if (MeshData->VertexBuffer == nullptr)
        {
            delete MeshData;
            return nullptr;
        }
        return MeshData;
    }
    
    void DestroyStaticMeshData(FStaticMeshData*& MeshData, URenderer& InRenderer)
    {
        if (MeshData == nullptr)
        {
            return;
        }
        if (MeshData->VertexBuffer != nullptr)
        {
            InRenderer.ReleaseVertexBuffer(MeshData->VertexBuffer);
            MeshData->VertexBuffer = nullptr;
        }
        delete MeshData;
        MeshData = nullptr;
    }
}

bool FEditorEngine::Init(HINSTANCE hInstance)
{
   
    Renderer = new URenderer();
    ImGuiRenderer = new FImGuiRenderer();
    WindowsWindow = new FWindowsWindow();
    WindowsWindow->Create(hInstance, 1920, 1080);
    RECT ClientRect = {};
    GetClientRect(WindowsWindow->GetHWND(), &ClientRect);
    const uint32 InitialWidth = ClientRect.right > ClientRect.left ? static_cast<uint32>(ClientRect.right - ClientRect.left) : 1;
    const uint32 InitialHeight = ClientRect.bottom > ClientRect.top ? static_cast<uint32>(ClientRect.bottom - ClientRect.top) : 1;  
    {
        Renderer->Create(WindowsWindow->GetHWND());
        Renderer->CreateShader();
        Renderer->CreateConstantBuffer();
        Renderer->CreateViewportRenderTarget(InitialWidth, InitialHeight);
        CubeMeshData = CreateStaticMeshData(UCubeComp::CreateMeshVertices(), *Renderer);
        SphereMeshData = CreateStaticMeshData(USphereComp::CreateMeshVertices(), *Renderer);
        if (CubeMeshData == nullptr || SphereMeshData == nullptr)
        {
            UE_LOG(Error, "Failed to create primitive mesh resources");
            return false;
        }
    }
    ImGuiRenderer->Init(WindowsWindow->GetHWND(), Renderer->GetDevice(), Renderer->GetDeviceContext());
    
    ViewportPanel = new FViewportPanel();
    ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
    ImGuiRenderer->GetPanelManager()->AddPanel(ViewportPanel);
    ConsolePanel = new FConsolePanel();
    ImGuiRenderer->GetPanelManager()->AddPanel(ConsolePanel);
    ObjectListPanel = new FObjectListPanel(&ObjectContext, CubeMeshData, SphereMeshData);
    ImGuiRenderer->GetPanelManager()->AddPanel(ObjectListPanel);
    DetailsPanel = new FDetailsPanel(&ObjectContext);
    ImGuiRenderer->GetPanelManager()->AddPanel(DetailsPanel);
    bIsRunning = true;
    UE_LOG(Info, "Info");
    UE_LOG(Error, "Error");
    UE_LOG(Warning, "Warning");
    return true;
}

void FEditorEngine::Run()
{

    double PreviousTime = FPlatformTime::Seconds();
    UCameraComponent* MainCamera = new UCameraComponent();
    
    if (Renderer->GetViewportWidth() > 0 && Renderer->GetViewportHeight() > 0)
    {
        const float AspectRatio = static_cast<float>(Renderer->GetViewportWidth()) / static_cast<float>(Renderer->GetViewportHeight());
        MainCamera->SetAspectRatio(AspectRatio);
    }
    
    while (bIsRunning == true)
    {
        const double CurrentTime = FPlatformTime::Seconds();
        const double DeltaTime = CurrentTime - PreviousTime;
        
        PreviousTime = CurrentTime;
        FrameStats.Update(DeltaTime);
        
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            
            //입력은 누가 가지냐
            if (msg.message == WM_QUIT)
            {
                bIsRunning = false;

                break;
            }
        }
        
        RECT ClientRect = {};
        if (GetClientRect(WindowsWindow->GetHWND(), &ClientRect))
        {
            const LONG ClientWidth = ClientRect.right - ClientRect.left;
            const LONG ClientHeight = ClientRect.bottom - ClientRect.top;
            if (ClientWidth > 0 && ClientHeight > 0)
            {
                Renderer->ResizeBackBuffer(static_cast<uint32>(ClientWidth), static_cast<uint32>(ClientHeight));
            }
        }
        ProcessViewportPicking(*MainCamera);
        
        uint32 DesiredWidth = 0;
        uint32 DesiredHeight = 0;
        if (ViewportPanel->ConsumeDesiredSize(DesiredWidth, DesiredHeight))
        {
            Renderer->ResizeViewportRenderTarget(DesiredWidth, DesiredHeight);
            ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
        }
        
        const uint32 ViewportWidth = Renderer->GetViewportWidth();
        const uint32 ViewportHeight = Renderer->GetViewportHeight();
        if (ViewportWidth > 0 && ViewportHeight > 0)
        {
            const float AspectRatio = static_cast<float>(ViewportWidth) / static_cast<float>(ViewportHeight);
            MainCamera->SetAspectRatio(AspectRatio);
        }
        CameraController.Update(*MainCamera, *WindowsWindow, static_cast<float>(DeltaTime));
        
        {
            // 준비 작업
            Renderer->Prepare();
            Renderer->BeginViewportRender();
            Renderer->PrepareShader();
            const FMatrix View = MainCamera->GetViewMatrix();
            const FMatrix Projection = MainCamera->GetProjectionMatrix();
            
            for (UObject* Object : ObjectContext.Objects)
            {
                if (Object == nullptr)
                {
                    continue;
                }
                
                UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Object);
                
                if (Primitive == nullptr)
                {
                    continue;
                }
                
                const FStaticMeshData* MeshData = Primitive->GetStaticMeshData();
                
                if (MeshData == nullptr)
                {
                    continue;
                }
                
                if (MeshData->VertexBuffer == nullptr || MeshData->VertexCount == 0)
                {
                    continue;
                }
                
                const FMatrix World = Primitive->GetTransform().GetLocalMatrix();
                Renderer->UpdateConstant(World, View, Projection);
                Renderer->RenderPrimitive(MeshData->VertexBuffer, MeshData->VertexCount);
            }
            Renderer->EndViewportRender();            
        }

        ImGuiRenderer->OnRender();
        Renderer->SwapBuffer();
    }
    
   CameraController.Stop(*WindowsWindow);
    delete MainCamera;
    
}

void FEditorEngine::Shutdown()
{
    for (UObject* Object : ObjectContext.Objects)
    {
        if (Object != nullptr)
        {
            delete Object;
        }
    }
    ObjectContext.Objects.Reset();
    ObjectContext.SelectedObject = nullptr;
    ImGuiRenderer->Shutdown();
    DestroyStaticMeshData(CubeMeshData, *Renderer);
    DestroyStaticMeshData(SphereMeshData, *Renderer);
    Renderer->Shutdown();
}

void FEditorEngine::ProcessViewportPicking(const UCameraComponent& Camera)
{
    float U = 0.0f;
    float V = 0.0f;
    if (!ViewportPanel->ConsumeClick(U, V))
    {
        return;
    }
    
    const uint32 Width = Renderer->GetViewportWidth();
    const uint32 Height = Renderer->GetViewportHeight();
    
    if (Width == 0 || Height == 0)
    {
        ObjectContext.SelectedObject = nullptr;
        return;
    }
    
    ObjectContext.SelectedObject = EditorPicking::PickObject(ObjectContext, U, V, Width, Height, Camera.GetViewMatrix(), Camera.GetProjectionMatrix());
}
