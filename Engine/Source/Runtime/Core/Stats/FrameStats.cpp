#include "FrameStats.h"

void FFrameStats::Update(double DeltaTime)
{
    LastFrameTimeMs = DeltaTime * 1000.0;

    AccumulatedTime += DeltaTime;
    ++AccumulatedFrameCount;

    if (AccumulatedTime >= StatsInterval)
    {
        FPS = static_cast<double>(AccumulatedFrameCount) / AccumulatedTime;

        AverageFrameTimeMs = AccumulatedTime / static_cast<double>(AccumulatedFrameCount) * 1000.0;

        AccumulatedTime = 0.0f;
        AccumulatedFrameCount = 0;
    }
}
