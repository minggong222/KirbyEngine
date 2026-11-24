#pragma once

#include "Core/STL.h"

enum class EViewMode : uint8
{
    Lit = 0,
    Unlit = 1,
    Wireframe = 2
};

enum class EEngineShowFlag : uint64
{
    None = 0,
    Primitives = 1ull << 0,
    Grid = 1ull << 1,
    Axes = 1ull << 2,
    Bounds = 1ull << 3,
    BillboardText = 1ull << 4
};

constexpr EEngineShowFlag operator|(EEngineShowFlag Left, EEngineShowFlag Right)
{
    return static_cast<EEngineShowFlag>(static_cast<uint64>(Left) | static_cast<uint64>(Right));
}

class FEngineShowFlags
{
public:
    constexpr FEngineShowFlags() : Flags(static_cast<uint64>(EEngineShowFlag::Primitives) | static_cast<uint64>(EEngineShowFlag::Grid) | static_cast<uint64>(EEngineShowFlag::Axes)) {}
    explicit constexpr FEngineShowFlags(uint64 InFlags) : Flags(InFlags) {}

    constexpr bool IsEnabled(EEngineShowFlag Flag) const { return (Flags & static_cast<uint64>(Flag)) != 0; }
    constexpr void SetEnabled(EEngineShowFlag Flag, bool bEnabled) { Flags = bEnabled ? Flags | static_cast<uint64>(Flag) : Flags & ~static_cast<uint64>(Flag); }
    constexpr uint64 GetValue() const { return Flags; }
    constexpr void SetValue(uint64 InFlags) { Flags = InFlags; }

private:
    uint64 Flags = 0;
};
