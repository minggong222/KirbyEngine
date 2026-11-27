#pragma once

#include "Core/STL.h"

#include <functional>

// Rendering thread (Unreal's FRenderingThread / ENQUEUE_RENDER_COMMAND).
//
// The game thread queues commands; the rendering thread runs them in order and is the
// only thread that records GPU commands through GRHI. Commands capture what they need;
// data owned by the game thread may only be read while the game thread waits in
// FlushRenderingCommands (the editor flushes once per frame).
//
// If the rendering thread is not running, commands execute immediately on the caller.

using FRenderCommand = std::function<void()>;

void StartRenderingThread();
void StopRenderingThread();
bool IsRenderingThreadRunning();

void EnqueueRenderCommand(const char* DebugName, FRenderCommand Command);

// Blocks the calling (game) thread until every queued command has finished.
void FlushRenderingCommands();

bool IsInRenderingThread();
bool IsInGameThread();

// Usage: ENQUEUE_RENDER_COMMAND(DrawViewport)([Captures]() { ... });
#define ENQUEUE_RENDER_COMMAND(Name) [](FRenderCommand&& Command) { EnqueueRenderCommand(#Name, std::move(Command)); }
