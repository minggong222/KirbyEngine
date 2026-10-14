#pragma once
#include "Core/STL.h"

enum class ELogVerbosity : uint8
{
    Info,
    Error,
    Warning,
    Input
};

using FLogOutputCallback = void(*)(void* UserData, ELogVerbosity Verbosity, const char* Message);
class FLog
{
public:
    static void Init(FLogOutputCallback Callback, void* UserData);
    static void Write(ELogVerbosity Verbosity, const char* Format, ...);
private:
    static FLogOutputCallback OutputCallback;
    static void* OutputUserData;
};

#define UE_LOG(Verbosity, ...) \
    FLog::Write(ELogVerbosity::Verbosity, __VA_ARGS__)
