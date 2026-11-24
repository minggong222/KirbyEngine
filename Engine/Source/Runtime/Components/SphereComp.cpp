#include "SphereComp.h"

#include <cmath>

FStaticMeshBuildData USphereComp::CreateMeshData()
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr int32 NumStacks = 20;
    constexpr int32 NumSlices = 20;
    constexpr int32 RowSize = NumSlices + 1;

    FStaticMeshBuildData Data;

    Data.Vertices.Reserve((NumStacks + 1) * (NumSlices + 1));
    Data.Indices.Reserve(2280);

    const auto MakeVertex = [](float Phi, float Theta, float U, float V)
    {
        const float SinPhi = std::sin(Phi);

        const float X = SinPhi * std::cos(Theta);
        const float Y = std::cos(Phi);
        const float Z = SinPhi * std::sin(Theta);

        return FVertexSimple{X, Y, Z, X * 0.5f + 0.5f, Y * 0.5f + 0.5f, Z * 0.5f + 0.5f, 1.0f, U, V};
    };

    for (int32 Stack = 0; Stack <= NumStacks; ++Stack)
    {
        const float Phi = PI * static_cast<float>(Stack) / static_cast<float>(NumStacks);

        for (int32 Slice = 0; Slice <= NumSlices; ++Slice)
        {
            const float Theta = 2.0f * PI * static_cast<float>(Slice) / static_cast<float>(NumSlices);

            const float U = static_cast<float>(Slice) / static_cast<float>(NumSlices);
            const float V = static_cast<float>(Stack) / static_cast<float>(NumStacks);

            Data.Vertices.Add(MakeVertex(Phi, Theta, U, V));
        }
    }

    for (int32 Stack = 0; Stack < NumStacks; ++Stack)
    {
        for (int32 Slice = 0; Slice < NumSlices; ++Slice)
        {
            const uint32 V00 = static_cast<uint32>(Stack * RowSize + Slice);
            const uint32 V01 = static_cast<uint32>(Stack * RowSize + Slice + 1);
            const uint32 V10 = static_cast<uint32>((Stack + 1) * RowSize + Slice);
            const uint32 V11 = static_cast<uint32>((Stack + 1) * RowSize + Slice + 1);

            if (Stack == 0)
            {
                Data.Indices.Add(V00);
                Data.Indices.Add(V11);
                Data.Indices.Add(V10);
            }
            else if (Stack == NumStacks - 1)
            {
                Data.Indices.Add(V00);
                Data.Indices.Add(V01);
                Data.Indices.Add(V11);
            }
            else
            {
                Data.Indices.Add(V00);
                Data.Indices.Add(V11);
                Data.Indices.Add(V10);

                Data.Indices.Add(V00);
                Data.Indices.Add(V01);
                Data.Indices.Add(V11);
            }
        }
    }

    return Data;
}