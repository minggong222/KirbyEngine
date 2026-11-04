#pragma once

class FAssetManager;
class URenderer;

namespace  BuiltinAssetPaths
{
    inline constexpr char Cube[] = "/Engine/BasicShapes/Cube";
    inline constexpr char Sphere[] = "/Engine/BasicShapes/Sphere";
}

bool RegisterBuiltinStaticMeshes(FAssetManager& AssetManager, URenderer& Renderer);