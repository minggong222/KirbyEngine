#pragma once

class FAssetManager;
class FRHI;

namespace BuiltinAssetPaths
{
    inline constexpr char Cube[] = "/Engine/BasicShapes/Cube";
    inline constexpr char Sphere[] = "/Engine/BasicShapes/Sphere";
    inline constexpr char Plane[] = "/Engine/BasicShapes/Plane";
    inline constexpr char Cone[] = "/Engine/BasicShapes/Cone";
    inline constexpr char Cylinder[] = "/Engine/BasicShapes/Cylinder";
    inline constexpr char Capsule[] = "/Engine/BasicShapes/Capsule";
}

bool RegisterBuiltinStaticMeshes(FAssetManager& AssetManager, FRHI& Renderer);