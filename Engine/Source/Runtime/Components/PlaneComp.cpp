#include "PlaneComp.h"

FStaticMeshBuildData UPlaneComp::CreateMeshData()
{
    FStaticMeshBuildData Data;

    Data.Vertices =
    {
            {-0.5f, -0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
            {0.5f, -0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
            {0.5f, 0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
            {-0.5f, 0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f}
    };

    Data.Indices =
    {
        0, 1, 2,
        0, 2, 3,

        1, 0, 2,
        2, 0, 3
    };

    return Data;
}