#pragma once

class FAssetManager;
class URenderer;

namespace BuiltinAssetPaths
{
    inline constexpr char Cube[] = "/Engine/BasicShapes/Cube";
    inline constexpr char Sphere[] = "/Engine/BasicShapes/Sphere";
    inline constexpr char Plane[] = "/Engine/BasicShapes/Plane";
}

bool RegisterBuiltinStaticMeshes(FAssetManager& AssetManager, URenderer& Renderer);