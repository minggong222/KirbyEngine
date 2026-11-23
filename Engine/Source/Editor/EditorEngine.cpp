#include "EditorEngine.h"

#include "EditorCameraController.h"
#include "EditorObjectPicker.h"
#include "Components/CameraComponent.h"
#include "Components/SphereComp.h"
#include "Components/CubeComp.h"
#include "Components/StaticMeshComponent.h"
#include "Core/STL.h"
#include "Core/Math/MathUtility.h"
#include "Core/Time/PlatformTime.h"
#include "Resources/PrimitiveMeshAssets.h"
#include "UI/Panels/DetailsPanel.h"
#include "UI/Panels/ObjectListPanel.h"
#include "UI/Panels/MainMenuPanel.h"
#include "UI/Panels/StatsPanel.h"
#include "UObject/Object.h"
#include "Resources/StaticMesh.h"
#include "EditorSceneSerializer.h"
#include "OBJStaticMeshImporter.h"

#include <filesystem>
#include <fstream>
#include <cmath>
#include <unordered_set>

namespace 
{
    void ValidateEditorObjectContext(const FEditorObjectContext& Context)
    {
#if defined(_DEBUG)
        std::unordered_set<const UObject*> UniqueObjects;

        for (UObject* Object : Context.Objects)
        {
            assert(Object != nullptr);
            assert(UniqueObjects.insert(Object).second);
        }

        if (Context.SelectedObject != nullptr)
        {
            assert(UniqueObjects.contains(Context.SelectedObject));
        }
#endif
    }
    bool RunEditorDebugValidation(FAssetManager& AssetManager, URenderer& Renderer)
    {
#if defined(_DEBUG)
    auto DestroyContextObjects = [](FEditorObjectContext& Context)
    {
        for (UObject* Object : Context.Objects)
        {
            delete Object;
        }

        Context.Objects.Reset();
        Context.SelectedObject = nullptr;
    };

    UStaticMesh* CubeMesh = AssetManager.Find<UStaticMesh>(BuiltinAssetPaths::Cube);

    if (CubeMesh == nullptr)
    {
        return false;
    }

    // ============================================================
    // Undo / Redo / Clipboard Test
    // ============================================================

    FEditorObjectContext UndoContext;
    FEditorUndoManager TestUndoManager;
    UndoContext.UndoManager = &TestUndoManager;

    UStaticMeshComponent* TestComponent = NewObject<UStaticMeshComponent>();

    if (TestComponent == nullptr)
    {
        return false;
    }

    TestComponent->SetStaticMesh(CubeMesh);

    UndoContext.Objects.Add(TestComponent);
    UndoContext.SelectedObject = TestComponent;

    const uint32 OriginalUUID = TestComponent->GetUUID();

    TestUndoManager.RecordCreate(UndoContext, TestComponent, 0);
    TestUndoManager.Undo(UndoContext);

    if (!UndoContext.Objects.IsEmpty())
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    TestUndoManager.Redo(UndoContext);

    if (UndoContext.Objects.Num() != 1 || UndoContext.Objects[0]->GetUUID() != OriginalUUID)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    // Clipboard Copy / Paste
    if (!TestUndoManager.CopySelected(UndoContext))
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    UObject* PastedObject = TestUndoManager.PasteCopied(UndoContext);

    if (PastedObject == nullptr || UndoContext.Objects.Num() != 2 || PastedObject->GetUUID() == OriginalUUID)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    UStaticMeshComponent* PastedComponent = Cast<UStaticMeshComponent>(PastedObject);

    if (PastedComponent == nullptr || PastedComponent->GetStaticMesh() != CubeMesh)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    TestUndoManager.Undo(UndoContext);

    if (UndoContext.Objects.Num() != 1)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    TestUndoManager.Redo(UndoContext);

    if (UndoContext.Objects.Num() != 2)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    // Transform Undo / Redo
    UndoContext.SelectedObject = UndoContext.Objects[0];

    UStaticMeshComponent* TransformTarget = Cast<UStaticMeshComponent>(UndoContext.SelectedObject);

    if (TransformTarget == nullptr)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    const FTransform BeforeTransform = TransformTarget->GetWorldTransform();
    FTransform AfterTransform = BeforeTransform;
    AfterTransform.Location.x += 2.0f;

    if (!TransformTarget->SetWorldTransform(AfterTransform))
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    TestUndoManager.RecordTransform(UndoContext, TransformTarget->GetUUID(), BeforeTransform, AfterTransform);
    TestUndoManager.Undo(UndoContext);

    TransformTarget = Cast<UStaticMeshComponent>(UndoContext.SelectedObject);

    if (TransformTarget == nullptr || std::fabs(TransformTarget->GetWorldTransform().Location.x - BeforeTransform.Location.x) > 0.0001f)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    TestUndoManager.Redo(UndoContext);

    TransformTarget = Cast<UStaticMeshComponent>(UndoContext.SelectedObject);

    if (TransformTarget == nullptr || std::fabs(TransformTarget->GetWorldTransform().Location.x - AfterTransform.Location.x) > 0.0001f)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    // Delete Undo / Redo
    UObject* DeleteTarget = UndoContext.Objects[1];
    UndoContext.SelectedObject = DeleteTarget;

    TestUndoManager.RecordDelete(UndoContext, DeleteTarget);

    const int32 DeleteIndex = UndoContext.Objects.Find(DeleteTarget);

    if (DeleteIndex == INDEX_NONE)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    UndoContext.Objects.RemoveAt(DeleteIndex);
    UndoContext.SelectedObject = nullptr;
    delete DeleteTarget;

    TestUndoManager.Undo(UndoContext);

    if (UndoContext.Objects.Num() != 2)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    TestUndoManager.Redo(UndoContext);

    if (UndoContext.Objects.Num() != 1)
    {
        DestroyContextObjects(UndoContext);
        return false;
    }

    TestUndoManager.Clear();
    DestroyContextObjects(UndoContext);

    // ============================================================
    // Scene Save / Load Test
    // ============================================================

    FEditorObjectContext SceneContext;

    UStaticMeshComponent* SceneMesh = NewObject<UStaticMeshComponent>();
    UCameraComponent* SceneCamera = NewObject<UCameraComponent>();
    UCameraComponent* TestViewportCamera = NewObject<UCameraComponent>();

    if (SceneMesh == nullptr || SceneCamera == nullptr || TestViewportCamera == nullptr)
    {
        delete SceneMesh;
        delete SceneCamera;
        delete TestViewportCamera;
        return false;
    }

    SceneMesh->SetStaticMesh(CubeMesh);

    FTransform MeshTransform = SceneMesh->GetTransform();
    MeshTransform.Location.x = 5.0f;
    SceneMesh->SetTransform(MeshTransform);

    SceneContext.Objects.Add(SceneMesh);
    SceneContext.Objects.Add(SceneCamera);

    const std::filesystem::path ScenePath = std::filesystem::temp_directory_path() / ("KirbyEngine_Validation_" + std::to_string(GetCurrentProcessId()) + ".Scene");

    auto RemoveTestScene = [&ScenePath]()
    {
        std::error_code Error;
        std::filesystem::remove(ScenePath, Error);
    };

    if (!FSceneSerializer::Save(ScenePath, SceneContext, *TestViewportCamera))
    {
        DestroyContextObjects(SceneContext);
        delete TestViewportCamera;
        RemoveTestScene();
        return false;
    }

    if (!FSceneSerializer::Load(ScenePath, SceneContext, AssetManager, *TestViewportCamera, Renderer))
    {
        DestroyContextObjects(SceneContext);
        delete TestViewportCamera;
        RemoveTestScene();
        return false;
    }

    if (SceneContext.Objects.Num() != 2 || Cast<UStaticMeshComponent>(SceneContext.Objects[0]) == nullptr || Cast<UCameraComponent>(SceneContext.Objects[1]) == nullptr)
    {
        DestroyContextObjects(SceneContext);
        delete TestViewportCamera;
        RemoveTestScene();
        return false;
    }

    if (Cast<UStaticMeshComponent>(SceneContext.Objects[0])->GetStaticMesh() != CubeMesh)
    {
        DestroyContextObjects(SceneContext);
        delete TestViewportCamera;
        RemoveTestScene();
        return false;
    }

    // 잘못된 Scene Load가 기존 Scene 상태를 변경하지 않는지 확인
    UObject* ExistingObject = SceneContext.Objects[0];
    const uint32 NextUUIDBeforeInvalidLoad = FEngineStatics::NextUUID;

    {
        std::ofstream InvalidScene(ScenePath, std::ios::binary | std::ios::trunc);

        InvalidScene << R"({"Version":999,"NextUUID":1,"Primitives":{},"Cameras":{}})";

        if (!InvalidScene.good())
        {
            DestroyContextObjects(SceneContext);
            delete TestViewportCamera;
            RemoveTestScene();
            return false;
        }
    }

    const bool bInvalidLoadRejected = !FSceneSerializer::Load(ScenePath, SceneContext, AssetManager, *TestViewportCamera, Renderer);
    const bool bSceneStatePreserved = SceneContext.Objects.Num() == 2 && SceneContext.Objects[0] == ExistingObject && FEngineStatics::NextUUID == NextUUIDBeforeInvalidLoad;

    DestroyContextObjects(SceneContext);
    delete TestViewportCamera;
    RemoveTestScene();

    if (!bInvalidLoadRejected || !bSceneStatePreserved)
    {
        return false;
    }

    // ============================================================
    // OBJ Parser Test
    // ============================================================

    if (!OBJImporter::RunParserValidation())
    {
        return false;
    }

#endif

    return true;
    }
}

bool FEditorEngine::Init(HINSTANCE hInstance)
{
   
    Renderer = new URenderer();
    ImGuiRenderer = new FImGuiRenderer();
    WindowsWindow = new FWindowsWindow();

    if (WindowsWindow == nullptr || !WindowsWindow->Create(hInstance, 1920, 1080))
    {
        Shutdown();
        return false;
    }
    RECT ClientRect = {};
    GetClientRect(WindowsWindow->GetHWND(), &ClientRect);
    const uint32 InitialWidth = ClientRect.right > ClientRect.left ? static_cast<uint32>(ClientRect.right - ClientRect.left) : 1;
    const uint32 InitialHeight = ClientRect.bottom > ClientRect.top ? static_cast<uint32>(ClientRect.bottom - ClientRect.top) : 1;  
    {
        Renderer->Create(WindowsWindow->GetHWND());
        Renderer->CreateShader();
        Renderer->CreateConstantBuffer();
        Renderer->CreateViewportRenderTarget(InitialWidth, InitialHeight);

        if (!Renderer->IsReady())
        {
            UE_LOG(Error, "Renderer initialization failed");
            Shutdown();
            return false;
        }
        
        if (!RegisterBuiltinStaticMeshes(AssetManager, *Renderer))
        {
            UE_LOG(Error, "Failed to register builtin static meshes");
            Shutdown();
            return false;
        }
    }
#if defined(_DEBUG)
    if (!RunEditorDebugValidation(AssetManager, *Renderer))
    {
        UE_LOG(Error, "Editor debug validation failed");
        Shutdown();
        return false;
    }

    UE_LOG(Info, "Editor debug validation passed");
#endif
    if (!ImGuiRenderer->Init(WindowsWindow->GetHWND(), Renderer->GetDevice(), Renderer->GetDeviceContext()))
    {
        UE_LOG(Error, "ImGui initialization failed");
        Shutdown();
        return false;
    }
    
    MainCamera = new UCameraComponent();
    
    ViewportPanel = new FViewportPanel();
    ViewportPanel->SetViewportTexture(Renderer->GetViewportSRV());
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(ViewportPanel))
    {
        ViewportPanel = nullptr;
        Shutdown();
        return false;
    }
    
    ConsolePanel = new FConsolePanel();
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(ConsolePanel))
    {
        ConsolePanel = nullptr;
        Shutdown();
        return false;
    }
    
    ObjectListPanel = new FObjectListPanel(&ObjectContext, &AssetManager, ViewportPanel, Renderer);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(ObjectListPanel))
    {
        ObjectListPanel = nullptr;
        Shutdown();
        return false;
    }
    
    DetailsPanel = new FDetailsPanel(&ObjectContext);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(DetailsPanel))
    {
        DetailsPanel = nullptr;
        Shutdown();
        return false;
    }
    
    StatsPanel = new FStatsPanel(&FrameStats, &ObjectContext);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(StatsPanel))
    {
        StatsPanel = nullptr;
        Shutdown();
        return false;
    }
    
    ConsolePanel->SetStatsPanel(StatsPanel);
    MainMenuPanel = new FMainMenuPanel(&ObjectContext, &AssetManager, WindowsWindow->GetHWND(), MainCamera, Renderer);
    if (!ImGuiRenderer->GetPanelManager()->AddPanel(MainMenuPanel))
    {
        MainMenuPanel = nullptr;
        Shutdown();
        return false;
    }
    
    ObjectContext.UndoManager = &UndoManager;
    
    bIsRunning = true;
    UE_LOG(Info, "Info");
    UE_LOG(Error, "Error");
    UE_LOG(Warning, "Warning");
    return true;
}

void FEditorEngine::Run()
{

    double PreviousTime = FPlatformTime::Seconds();
    
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
        CameraController.Update(*MainCamera, *WindowsWindow, static_cast<float>(DeltaTime), ViewportPanel->IsViewportHovered());
        
        const FMatrix View = MainCamera->GetViewMatrix();
        const FMatrix Projection = MainCamera->GetProjectionMatrix();
        
        ViewportPanel->SetGizmoContext(&ObjectContext, View, Projection);
        
        {
            // 준비 작업
            Renderer->Prepare();
            Renderer->BeginViewportRender();
            Renderer->PrepareShader();
            
            for (UObject* Object : ObjectContext.Objects)
            {
                if (Object == nullptr)
                {
                    continue;
                }
                
                UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Object);
                
                if (Component == nullptr)
                {
                    continue;
                }
                
                const UStaticMesh* Mesh  = Component->GetStaticMesh();
                
                if (Mesh  == nullptr)
                {
                    continue;
                }
                
                const FStaticMeshRenderData& RenderData = Mesh->GetRenderData();
                
                if (RenderData.VertexBuffer == nullptr || RenderData.VertexCount == 0)
                {
                    continue;
                }
                
                const FMatrix World = Component->GetWorldMatrix();
                Renderer->UpdateConstant(World, View, Projection, false);
                Renderer->RenderPrimitive(RenderData.VertexBuffer, RenderData.VertexCount);
            }
            
            UStaticMeshComponent* SelectedComponent = Cast<UStaticMeshComponent>(ObjectContext.SelectedObject);

            if (SelectedComponent != nullptr)
            {
                const UStaticMesh* SelectedMesh = SelectedComponent->GetStaticMesh();

                if (SelectedMesh != nullptr)
                {
                    const FStaticMeshRenderData& SelectedRenderData = SelectedMesh->GetRenderData();

                    if (SelectedRenderData.VertexBuffer != nullptr && SelectedRenderData.VertexCount > 0)
                    {
                        const FMatrix OutlineScale = FMatrix::CreateScale(FVector(1.03f, 1.03f, 1.03f));
                        const FMatrix OutlineWorld = OutlineScale * SelectedComponent->GetWorldMatrix();

                        Renderer->UpdateConstant(OutlineWorld, View, Projection, true);
                        Renderer->RenderPrimitive(SelectedRenderData.VertexBuffer, SelectedRenderData.VertexCount, true);
                    }
                }
            }
            
            Renderer->EndViewportRender();            
        }
        
        ImGuiRenderer->OnRender();

#if defined(_DEBUG)
        ValidateEditorObjectContext(ObjectContext);
#endif
        
        const ImGuiIO& IO = ImGui::GetIO();

        const bool bEditorFocused = WindowsWindow != nullptr && GetForegroundWindow() == WindowsWindow->GetHWND();
        const bool bCanProcessShortcut = bEditorFocused && !IO.WantTextInput && !ImGui::IsAnyItemActive();

        if (bCanProcessShortcut)
        {
            if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C, false))
            {
                UndoManager.CopySelected(ObjectContext);
            }
            else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
            {
                UndoManager.PasteCopied(ObjectContext);
            }
            else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W, false))
            {
                ObjectListPanel->DuplicateSelectedObject();
            }
            else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                UndoManager.Undo(ObjectContext);
            }
            else if (IO.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
            {
                UndoManager.Redo(ObjectContext);
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            {
                ObjectListPanel->DeleteSelectedObject();
            }
        }
        
        
        Renderer->SwapBuffer();
    }
    
   CameraController.Stop(*WindowsWindow);
}

void FEditorEngine::Shutdown()
{
    bIsRunning = false;

    if (WindowsWindow != nullptr)
    {
        CameraController.Stop(*WindowsWindow);
    }

    if (ImGuiRenderer != nullptr)
    {
        ImGuiRenderer->Shutdown();
        delete ImGuiRenderer;
        ImGuiRenderer = nullptr;
    }

    ViewportPanel = nullptr;
    ConsolePanel = nullptr;
    ObjectListPanel = nullptr;
    DetailsPanel = nullptr;
    MainMenuPanel = nullptr;
    StatsPanel = nullptr;

    for (UObject* Object : ObjectContext.Objects)
    {
        delete Object;
    }

    ObjectContext.Objects.Reset();
    ObjectContext.SelectedObject = nullptr;

    if (MainCamera != nullptr)
    {
        delete MainCamera;
        MainCamera = nullptr;
    }

    UndoManager.Clear();
    
    AssetManager.Shutdown();

    if (Renderer != nullptr)
    {
        Renderer->Shutdown();
        delete Renderer;
        Renderer = nullptr;
    }

    if (WindowsWindow != nullptr)
    {
        WindowsWindow->Destroy();
        delete WindowsWindow;
        WindowsWindow = nullptr;
    }
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
