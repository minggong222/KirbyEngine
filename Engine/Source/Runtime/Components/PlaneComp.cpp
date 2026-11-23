#include "PlaneComp.h"

TArray<FVertexSimple> UPlaneComp::CreateMeshVertices()
{
    return {
        // Front face: +Z
        {-0.5f, -0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
        { 0.5f, -0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},

        {-0.5f, -0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},

        // Back face: -Z winding
        { 0.5f, -0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
        {-0.5f, -0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
        { 0.5f,  0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},

        { 0.5f,  0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
        {-0.5f, -0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f},
        {-0.5f,  0.5f, 0.0f, 0.3f, 0.7f, 1.0f, 1.0f}
    };
}