#pragma once
#include "Core/STL.h"

class FFrameStats
{
public:
    void Update(double DeltaTime);

    double GetFPS() const { return FPS; }
    double GetFrameTimeMs() const { return LastFrameTimeMs; }
    double GetAverageFrameTimeMs() const { return AverageFrameTimeMs; }
private:
    static constexpr double StatsInterval = 1.0;

    double AccumulatedTime = 0.0;
    uint64 AccumulatedFrameCount = 0;

    double FPS = 0.0;
    double LastFrameTimeMs = 0.0;
    double AverageFrameTimeMs = 0.0;
};
