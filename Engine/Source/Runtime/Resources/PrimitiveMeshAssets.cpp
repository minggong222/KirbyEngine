#include "PrimitiveMeshAssets.h"

#include "AssetManager.h"
#include "StaticMesh.h"

#include "Renderer/Renderer.h"

#include "Components/CubeComp.h"
#include "Components/SphereComp.h"
#include "Components/PlaneComp.h"

#include "UObject/Class.h"

namespace 
{
    UStaticMesh* CreatePrimitiveMesh(URenderer& Renderer, FStaticMeshBuildData&& BuildData)
    {
        UStaticMesh* Mesh = NewObject<UStaticMesh>();

        if (Mesh == nullptr)
        {
            return nullptr;
        }

        if (!Mesh->Initialize(Renderer, std::move(BuildData)))
        {
            delete Mesh;
            return nullptr;
        }

        return Mesh;
    }
}

bool RegisterBuiltinStaticMeshes(FAssetManager& AssetManager, URenderer& Renderer)
{
    UStaticMesh* Cube = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Cube, [&Renderer]()
    {
        return CreatePrimitiveMesh(Renderer, UCubeComp::CreateMeshData());
    });
    
    UStaticMesh* Sphere = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Sphere, [&Renderer]()
    {
        return CreatePrimitiveMesh(Renderer, USphereComp::CreateMeshData());
    });
    
    UStaticMesh* Plane = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Plane, [&Renderer]()
    {
        return CreatePrimitiveMesh(Renderer, UPlaneComp::CreateMeshData());
    });
    
    return Cube != nullptr && Sphere != nullptr && Plane != nullptr;
}
