#include "PrimitiveMeshAssets.h"

#include "AssetManager.h"
#include "StaticMesh.h"

#include "Renderer/Renderer.h"

#include "Components/CubeComp.h"
#include "Components/SphereComp.h"

#include "UObject/Class.h"

namespace 
{
    UStaticMesh* CreatePrimitiveMesh(URenderer& Renderer, TArray<FVertexSimple>&& Vertices)
    {
        UStaticMesh* Mesh = NewObject<UStaticMesh>();
        if (Mesh == nullptr)
        {
            return nullptr;
        }
        if (!Mesh->Initialize(Renderer, std::move(Vertices)))
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
        return CreatePrimitiveMesh(Renderer, UCubeComp::CreateMeshVertices());
    });
    
    UStaticMesh* Sphere = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Sphere, [&Renderer]()
    {
        return CreatePrimitiveMesh(Renderer, USphereComp::CreateMeshVertices());
    });
    
    return Cube != nullptr && Sphere != nullptr;
}
