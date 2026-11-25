#pragma once

#include "Core/Math/Math.h"

struct FVertexSimple
{
    float x, y, z;
    float r, g, b, a;
    float u, v;
    float nx = 0.0f;
    float ny = 0.0f;
    float nz = 0.0f;
};