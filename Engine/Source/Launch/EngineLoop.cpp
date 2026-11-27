#include "EngineLoop.h"

#include "Engine/Engine.h"
#include "Core/Time/PlatformTime.h"
#include "RenderCore/RenderingThread.h"

// Launch is the only place that knows which concrete engine to run.
#include "EditorEngine.h"

bool FEngineLoop::PreInit(HINSTANCE hInstance)
{
    Engine = new FEditorEngine();
    GEngine = Engine;

    if (!Engine->Init(hInstance))
    {
        // Init already cleaned up after itself (it calls Shutdown on failure).
        delete Engine;
        Engine = nullptr;
        GEngine = nullptr;
        return false;
    }

    // From here on GPU commands are recorded on the rendering thread.
    StartRenderingThread();

    PreviousTime = FPlatformTime::Seconds();
    return true;
}

void FEngineLoop::Tick()
{
    if (Engine == nullptr)
    {
        return;
    }

    PumpMessages();

    if (Engine->IsExitRequested())
    {
        return;
    }

    const double CurrentTime = FPlatformTime::Seconds();
    const double DeltaTime = CurrentTime - PreviousTime;
    PreviousTime = CurrentTime;

    Engine->Tick(static_cast<float>(DeltaTime));
}

void FEngineLoop::Exit()
{
    // Finish outstanding GPU work and stop the rendering thread before resources are released.
    FlushRenderingCommands();
    StopRenderingThread();

    if (Engine != nullptr)
    {
        Engine->Shutdown();
        delete Engine;
        Engine = nullptr;
    }

    GEngine = nullptr;
}

bool FEngineLoop::ShouldExit() const
{
    return Engine == nullptr || Engine->IsExitRequested();
}

void FEngineLoop::PumpMessages()
{
    MSG Message;

    while (PeekMessage(&Message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Message);
        DispatchMessage(&Message);

        if (Message.message == WM_QUIT)
        {
            Engine->RequestExit();
            return;
        }
    }
}
