#pragma once

#include "Core/STL.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>

// Binary archive (Unreal's FArchive). The same operator<< code reads or writes,
// depending on IsLoading(), so each type describes its layout once.
class FArchive
{
public:
    virtual ~FArchive() = default;

    bool IsLoading() const { return bLoading; }
    bool IsSaving() const { return !bLoading; }

    bool IsError() const { return bError; }
    void SetError() { bError = true; }

    // Reads into or writes from Data.
    virtual void Serialize(void* Data, uint64 Size) = 0;

    // Plain data: numbers, enums and trivially copyable structs (FVector, FVertexSimple...).
    template<typename T>
    std::enable_if_t<std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>, FArchive&> operator<<(T& Value)
    {
        Serialize(&Value, sizeof(T));
        return *this;
    }

    FArchive& operator<<(std::string& Value)
    {
        uint32 Length = static_cast<uint32>(Value.size());
        *this << Length;

        if (IsLoading())
        {
            if (Length > MaxStringLength)
            {
                SetError();
                return *this;
            }

            Value.resize(Length);
        }

        if (Length > 0)
        {
            Serialize(Value.data(), Length);
        }

        return *this;
    }

    FArchive& operator<<(FString& Value)
    {
        return *this << Value.ToStd();
    }

    template<typename T>
    FArchive& operator<<(TArray<T>& Array)
    {
        uint32 Count = static_cast<uint32>(Array.Num());
        *this << Count;

        if (IsLoading())
        {
            if (Count > MaxArrayCount)
            {
                SetError();
                return *this;
            }

            Array.SetNum(static_cast<int32>(Count));
        }

        if constexpr (std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>)
        {
            if (Count > 0)
            {
                Serialize(Array.GetData(), sizeof(T) * static_cast<uint64>(Count));
            }
        }
        else
        {
            for (T& Element : Array)
            {
                *this << Element;

                if (IsError())
                {
                    break;
                }
            }
        }

        return *this;
    }

protected:
    explicit FArchive(bool bInLoading) : bLoading(bInLoading) {}

private:
    static constexpr uint32 MaxStringLength = 64u * 1024u * 1024u;
    static constexpr uint32 MaxArrayCount = 512u * 1024u * 1024u;

    bool bLoading = false;
    bool bError = false;
};

class FArchiveFileWriter final : public FArchive
{
public:
    explicit FArchiveFileWriter(const std::filesystem::path& Path)
        : FArchive(false), Stream(Path, std::ios::binary | std::ios::trunc)
    {
        if (!Stream.is_open())
        {
            SetError();
        }
    }

    void Serialize(void* Data, uint64 Size) override
    {
        if (IsError() || Size == 0)
        {
            return;
        }

        Stream.write(static_cast<const char*>(Data), static_cast<std::streamsize>(Size));

        if (!Stream)
        {
            SetError();
        }
    }

private:
    std::ofstream Stream;
};

class FArchiveFileReader final : public FArchive
{
public:
    explicit FArchiveFileReader(const std::filesystem::path& Path)
        : FArchive(true), Stream(Path, std::ios::binary)
    {
        if (!Stream.is_open())
        {
            SetError();
        }
    }

    void Serialize(void* Data, uint64 Size) override
    {
        if (IsError() || Size == 0)
        {
            return;
        }

        Stream.read(static_cast<char*>(Data), static_cast<std::streamsize>(Size));

        if (static_cast<uint64>(Stream.gcount()) != Size)
        {
            SetError();
        }
    }

private:
    std::ifstream Stream;
};
