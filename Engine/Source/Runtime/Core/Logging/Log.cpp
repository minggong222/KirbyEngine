#include "Log.h"

#include <cstdarg>

FLogOutputCallback FLog::OutputCallback = nullptr;
void * FLog::OutputUserData = nullptr;

void FLog::Init(FLogOutputCallback Callback, void* UserData)
{
    OutputCallback = Callback;
    OutputUserData = UserData;
}

void FLog::Write(ELogVerbosity Verbosity, const char* Format, ...)
{
    if (Format == nullptr) return;
    
    char Message[1024];
    
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Message, sizeof(Message), Format, Args);
    va_end(Args);
    
    if (OutputCallback != nullptr)
    {
        OutputCallback(OutputUserData, Verbosity, Message);
    }
}
