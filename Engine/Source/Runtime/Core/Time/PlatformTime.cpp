#include "PlatformTime.h"
#include "EditorEngine.h"

double FPlatformTime::Seconds()
{
    static const double SecondsPerCount = []()
    {
        LARGE_INTEGER Frequency;
        QueryPerformanceFrequency(&Frequency);
        
        return 1.0 / static_cast<double>(Frequency.QuadPart);
    }();

    LARGE_INTEGER Counter{};
    QueryPerformanceCounter(&Counter);
    return static_cast<double>(Counter.QuadPart) * SecondsPerCount;
}
