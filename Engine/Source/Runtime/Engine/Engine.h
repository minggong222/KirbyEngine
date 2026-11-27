#pragma once

#include "Core/STL.h"

class UWorld;

// Base engine, the counterpart of Unreal's UEngine.
// Runtime code only ever sees this interface; the concrete engine
// (FEditorEngine today, a game engine later) is chosen by the Launch module.
class FEngine
{
public:
    virtual ~FEngine() = default;

    virtual bool Init(HINSTANCE hInstance) = 0;
    virtual void Tick(float DeltaTime) = 0;
    virtual void Shutdown() = 0;

    // The world this engine is currently running.
    virtual UWorld* GetWorld() const { return nullptr; }

    void RequestExit() { bExitRequested = true; }
    bool IsExitRequested() const { return bExitRequested; }

private:
    bool bExitRequested = false;
};

// Global engine instance, set by FEngineLoop for the lifetime of the engine.
extern FEngine* GEngine;
