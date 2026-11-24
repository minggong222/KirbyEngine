#include "CubeComp.h"

FStaticMeshBuildData UCubeComp::CreateMeshData()
{
    FStaticMeshBuildData Data;

    auto AddQuad = [&Data](const FVertexSimple& V0, const FVertexSimple& V1, const FVertexSimple& V2, const FVertexSimple& V3)
    {
        const uint32 BaseIndex = static_cast<uint32>(Data.Vertices.Num());

        Data.Vertices.Add(V0);
        Data.Vertices.Add(V1);
        Data.Vertices.Add(V2);
        Data.Vertices.Add(V3);

        Data.Indices.Add(BaseIndex);
        Data.Indices.Add(BaseIndex + 1);
        Data.Indices.Add(BaseIndex + 2);

        Data.Indices.Add(BaseIndex);
        Data.Indices.Add(BaseIndex + 2);
        Data.Indices.Add(BaseIndex + 3);
    };

    // Front: +Z
    AddQuad(
        {-0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
        {0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f},
        {0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f},
        {-0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f}
    );

    // Back: -Z
    AddQuad(
        {0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
        {-0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        {-0.5f, 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f},
        {0.5f, 0.5f, -0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f}
    );

    // Left: -X
    AddQuad(
        {-0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f},
        {-0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f},
        {-0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f},
        {-0.5f, 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f}
    );

    // Right: +X
    AddQuad(
        {0.5f, -0.5f, -0.5f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f},
        {0.5f, 0.5f, -0.5f, 0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f},
        {0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.0f},
        {0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f}
    );

    // Top: +Y
    AddQuad(
        {-0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f},
        {-0.5f, 0.5f, 0.5f, 0.0f, 0.5f, 1.0f, 1.0f, 0.0f, 1.0f},
        {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f},
        {0.5f, 0.5f, -0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f}
    );

    // Bottom: -Y
    AddQuad(
        {-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f},
        {0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.5f, 1.0f, 1.0f, 0.0f},
        {0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f},
        {-0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f}
    );

    return Data;
}