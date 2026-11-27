#include "RenderCore/RenderingThread.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace
{
    struct FRenderingThreadState
    {
        std::thread Thread;
        std::mutex Mutex;
        std::condition_variable WorkAvailable;
        std::condition_variable WorkFinished;
        std::deque<FRenderCommand> Queue;

        uint64 EnqueuedCount = 0;
        uint64 CompletedCount = 0;

        bool bRunning = false;
        bool bStopRequested = false;

        std::thread::id GameThreadId = std::this_thread::get_id();
        std::thread::id RenderThreadId;
    };

    FRenderingThreadState& GetState()
    {
        static FRenderingThreadState State;
        return State;
    }

    void RenderingThreadMain()
    {
        FRenderingThreadState& State = GetState();

        for (;;)
        {
            FRenderCommand Command;

            {
                std::unique_lock<std::mutex> Lock(State.Mutex);
                State.WorkAvailable.wait(Lock, [&State]() { return State.bStopRequested || !State.Queue.empty(); });

                if (State.Queue.empty())
                {
                    return; // stop requested and nothing left to run
                }

                Command = std::move(State.Queue.front());
                State.Queue.pop_front();
            }

            Command();

            {
                std::lock_guard<std::mutex> Lock(State.Mutex);
                ++State.CompletedCount;
            }

            State.WorkFinished.notify_all();
        }
    }
}

void StartRenderingThread()
{
    FRenderingThreadState& State = GetState();

    if (State.bRunning)
    {
        return;
    }

    State.GameThreadId = std::this_thread::get_id();
    State.bStopRequested = false;
    State.bRunning = true;
    State.Thread = std::thread(RenderingThreadMain);
    State.RenderThreadId = State.Thread.get_id();
}

void StopRenderingThread()
{
    FRenderingThreadState& State = GetState();

    if (!State.bRunning)
    {
        return;
    }

    {
        std::lock_guard<std::mutex> Lock(State.Mutex);
        State.bStopRequested = true;
    }

    State.WorkAvailable.notify_all();
    State.Thread.join();

    State.bRunning = false;
    State.RenderThreadId = std::thread::id();
}

bool IsRenderingThreadRunning()
{
    return GetState().bRunning;
}

void EnqueueRenderCommand(const char* DebugName, FRenderCommand Command)
{
    (void)DebugName;
    FRenderingThreadState& State = GetState();

    if (!State.bRunning || IsInRenderingThread())
    {
        Command();
        return;
    }

    {
        std::lock_guard<std::mutex> Lock(State.Mutex);
        State.Queue.push_back(std::move(Command));
        ++State.EnqueuedCount;
    }

    State.WorkAvailable.notify_one();
}

void FlushRenderingCommands()
{
    FRenderingThreadState& State = GetState();

    if (!State.bRunning || IsInRenderingThread())
    {
        return;
    }

    std::unique_lock<std::mutex> Lock(State.Mutex);
    const uint64 Target = State.EnqueuedCount;
    State.WorkFinished.wait(Lock, [&State, Target]() { return State.CompletedCount >= Target; });
}

bool IsInRenderingThread()
{
    const FRenderingThreadState& State = GetState();
    return State.bRunning ? std::this_thread::get_id() == State.RenderThreadId : std::this_thread::get_id() == State.GameThreadId;
}

bool IsInGameThread()
{
    return std::this_thread::get_id() == GetState().GameThreadId;
}
