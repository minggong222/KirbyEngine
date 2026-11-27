#pragma once

#include "Core/STL.h"

class FEngine;

// Owns the engine instance and drives the frame loop, like Unreal's FEngineLoop:
// PreInit creates and initializes the engine, Tick pumps OS messages, measures
// the frame time and ticks the engine, Exit shuts everything down.
class FEngineLoop
{
public:
    bool PreInit(HINSTANCE hInstance);
    void Tick();
    void Exit();

    bool ShouldExit() const;

private:
    void PumpMessages();

    FEngine* Engine = nullptr;
    double PreviousTime = 0.0;
};
