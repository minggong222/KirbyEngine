#include "Log.h"

#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <mutex>

FLogOutputCallback FLog::OutputCallback = nullptr;
void * FLog::OutputUserData = nullptr;

namespace
{
    const char* GetVerbosityName(ELogVerbosity Verbosity)
    {
        switch (Verbosity)
        {
        case ELogVerbosity::Error: return "Error";
        case ELogVerbosity::Warning: return "Warning";
        case ELogVerbosity::Input: return "Input";
        default: return "Info";
        }
    }

    // Saved/Logs/KirbyEngine.log, recreated on each run (like Unreal's Saved/Logs).
    // Messages can come from the game and rendering threads, hence the lock.
    void WriteToLogFile(ELogVerbosity Verbosity, const char* Message)
    {
        static std::mutex FileMutex;
        static std::ofstream File = []()
        {
            std::error_code Error;
            std::filesystem::create_directories("Saved/Logs", Error);
            return std::ofstream("Saved/Logs/KirbyEngine.log", std::ios::trunc);
        }();

        std::lock_guard<std::mutex> Lock(FileMutex);

        if (File.is_open())
        {
            File << '[' << GetVerbosityName(Verbosity) << "] " << Message << '\n';
            File.flush();
        }
    }
}

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

    WriteToLogFile(Verbosity, Message);

    if (OutputCallback != nullptr)
    {
        OutputCallback(OutputUserData, Verbosity, Message);
    }
}
