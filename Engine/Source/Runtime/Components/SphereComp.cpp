
#include "SphereComp.h"
#include <cmath>

TArray<FVertexSimple> USphereComp::CreateMeshVertices()
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr int32 NumStacks = 20;
    constexpr int32 NumSlices = 20;

    TArray<FVertexSimple> Vertices;
    Vertices.Reserve(2280);

    const auto MakeVertex = [](float Phi, float Theta)
    {
        const float SinPhi = std::sin(Phi);

        const float X = SinPhi * std::cos(Theta);
        const float Y = std::cos(Phi);
        const float Z = SinPhi * std::sin(Theta);

        return FVertexSimple{
            X,
            Y,
            Z,
            X * 0.5f + 0.5f,
            Y * 0.5f + 0.5f,
            Z * 0.5f + 0.5f,
            1.0f
        };
    };

    for (int32 Stack = 0; Stack < NumStacks; ++Stack)
    {
        const float Phi0 = PI * static_cast<float>(Stack) / static_cast<float>(NumStacks);
        const float Phi1 = PI * static_cast<float>(Stack + 1) / static_cast<float>(NumStacks);

        for (int32 Slice = 0; Slice < NumSlices; ++Slice)
        {
            const int32 NextSlice = (Slice + 1) % NumSlices;

            const float Theta0 = 2.0f * PI * static_cast<float>(Slice) / static_cast<float>(NumSlices);
            const float Theta1 = 2.0f * PI * static_cast<float>(NextSlice) / static_cast<float>(NumSlices);

            const FVertexSimple V00 = MakeVertex(Phi0, Theta0);
            const FVertexSimple V10 = MakeVertex(Phi1, Theta0);
            const FVertexSimple V11 = MakeVertex(Phi1, Theta1);
            const FVertexSimple V01 = MakeVertex(Phi0, Theta1);

            if (Stack == 0)
            {
                // Top cap - Outward
                Vertices.Add(V00);
                Vertices.Add(V11);
                Vertices.Add(V10);
            }
            else if (Stack == NumStacks - 1)
            {
                // Bottom cap - Outward
                Vertices.Add(V00);
                Vertices.Add(V01);
                Vertices.Add(V11);
            }
            else
            {
                // First triangle - Outward
                Vertices.Add(V00);
                Vertices.Add(V11);
                Vertices.Add(V10);

                // Second triangle - Outward
                Vertices.Add(V00);
                Vertices.Add(V01);
                Vertices.Add(V11);
            }
        }
    }

    return Vertices;
}
