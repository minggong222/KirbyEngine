#include "PrimitiveMeshAssets.h"

#include "AssetManager.h"
#include "StaticMesh.h"
#include "Renderer/Renderer.h"

#include <cmath>
#include <vector>

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr int32 NumSlices = 32;

    FVertexSimple MakeVertex(float X, float Y, float Z, float U, float V)
    {
        return {X, Y, Z, 1.0f, 1.0f, 1.0f, 1.0f, U, V};
    }

    void AddQuad(FStaticMeshBuildData& Data, const FVertexSimple& V0, const FVertexSimple& V1, const FVertexSimple& V2, const FVertexSimple& V3)
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
    }

    FStaticMeshBuildData CreateCubeMeshData()
    {
        FStaticMeshBuildData Data;
        Data.Vertices.Reserve(24);
        Data.Indices.Reserve(36);

        // Front: +Z
        AddQuad(Data,
            MakeVertex(-0.5f, -0.5f, 0.5f, 0.0f, 1.0f),
            MakeVertex(0.5f, -0.5f, 0.5f, 1.0f, 1.0f),
            MakeVertex(0.5f, 0.5f, 0.5f, 1.0f, 0.0f),
            MakeVertex(-0.5f, 0.5f, 0.5f, 0.0f, 0.0f));

        // Back: -Z
        AddQuad(Data,
            MakeVertex(0.5f, -0.5f, -0.5f, 0.0f, 1.0f),
            MakeVertex(-0.5f, -0.5f, -0.5f, 1.0f, 1.0f),
            MakeVertex(-0.5f, 0.5f, -0.5f, 1.0f, 0.0f),
            MakeVertex(0.5f, 0.5f, -0.5f, 0.0f, 0.0f));

        // Left: -X
        AddQuad(Data,
            MakeVertex(-0.5f, -0.5f, -0.5f, 0.0f, 1.0f),
            MakeVertex(-0.5f, -0.5f, 0.5f, 1.0f, 1.0f),
            MakeVertex(-0.5f, 0.5f, 0.5f, 1.0f, 0.0f),
            MakeVertex(-0.5f, 0.5f, -0.5f, 0.0f, 0.0f));

        // Right: +X
        AddQuad(Data,
            MakeVertex(0.5f, -0.5f, -0.5f, 0.0f, 1.0f),
            MakeVertex(0.5f, 0.5f, -0.5f, 0.0f, 0.0f),
            MakeVertex(0.5f, 0.5f, 0.5f, 1.0f, 0.0f),
            MakeVertex(0.5f, -0.5f, 0.5f, 1.0f, 1.0f));

        // Top: +Y
        AddQuad(Data,
            MakeVertex(-0.5f, 0.5f, -0.5f, 0.0f, 0.0f),
            MakeVertex(-0.5f, 0.5f, 0.5f, 0.0f, 1.0f),
            MakeVertex(0.5f, 0.5f, 0.5f, 1.0f, 1.0f),
            MakeVertex(0.5f, 0.5f, -0.5f, 1.0f, 0.0f));

        // Bottom: -Y
        AddQuad(Data,
            MakeVertex(-0.5f, -0.5f, -0.5f, 0.0f, 0.0f),
            MakeVertex(0.5f, -0.5f, -0.5f, 1.0f, 0.0f),
            MakeVertex(0.5f, -0.5f, 0.5f, 1.0f, 1.0f),
            MakeVertex(-0.5f, -0.5f, 0.5f, 0.0f, 1.0f));

        return Data;
    }

    FStaticMeshBuildData CreateSphereMeshData()
    {
        constexpr int32 NumStacks = 20;
        constexpr int32 RowSize = NumSlices + 1;

        FStaticMeshBuildData Data;
        Data.Vertices.Reserve((NumStacks + 1) * RowSize);
        Data.Indices.Reserve(2280);

        for (int32 Stack = 0; Stack <= NumStacks; ++Stack)
        {
            const float Phi = PI * static_cast<float>(Stack) / static_cast<float>(NumStacks);

            for (int32 Slice = 0; Slice <= NumSlices; ++Slice)
            {
                const float Theta = 2.0f * PI * static_cast<float>(Slice) / static_cast<float>(NumSlices);
                const float SinPhi = std::sin(Phi);

                const float X = SinPhi * std::cos(Theta);
                const float Y = std::cos(Phi);
                const float Z = SinPhi * std::sin(Theta);

                const float U = static_cast<float>(Slice) / static_cast<float>(NumSlices);
                const float V = static_cast<float>(Stack) / static_cast<float>(NumStacks);

                Data.Vertices.Add({X, Y, Z, X * 0.5f + 0.5f, Y * 0.5f + 0.5f, Z * 0.5f + 0.5f, 1.0f, U, V});
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

    FStaticMeshBuildData CreatePlaneMeshData()
    {
        FStaticMeshBuildData Data;

        Data.Vertices =
        {
            MakeVertex(-0.5f, -0.5f, 0.0f, 0.0f, 1.0f),
            MakeVertex(0.5f, -0.5f, 0.0f, 1.0f, 1.0f),
            MakeVertex(0.5f, 0.5f, 0.0f, 1.0f, 0.0f),
            MakeVertex(-0.5f, 0.5f, 0.0f, 0.0f, 0.0f)
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

    FStaticMeshBuildData CreateCylinderMeshData()
    {
        constexpr float Radius = 0.5f;
        constexpr float HalfHeight = 0.5f;

        FStaticMeshBuildData Data;
        Data.Vertices.Reserve(NumSlices * 4 + (NumSlices + 2) * 2);
        Data.Indices.Reserve(NumSlices * 12);

        for (int32 Slice = 0; Slice < NumSlices; ++Slice)
        {
            const float U0 = static_cast<float>(Slice) / static_cast<float>(NumSlices);
            const float U1 = static_cast<float>(Slice + 1) / static_cast<float>(NumSlices);
            const float Theta0 = 2.0f * PI * U0;
            const float Theta1 = 2.0f * PI * U1;

            const float X0 = Radius * std::cos(Theta0);
            const float Z0 = Radius * std::sin(Theta0);
            const float X1 = Radius * std::cos(Theta1);
            const float Z1 = Radius * std::sin(Theta1);

            const uint32 BaseIndex = static_cast<uint32>(Data.Vertices.Num());

            Data.Vertices.Add(MakeVertex(X0, -HalfHeight, Z0, U0, 1.0f));
            Data.Vertices.Add(MakeVertex(X0, HalfHeight, Z0, U0, 0.0f));
            Data.Vertices.Add(MakeVertex(X1, -HalfHeight, Z1, U1, 1.0f));
            Data.Vertices.Add(MakeVertex(X1, HalfHeight, Z1, U1, 0.0f));

            Data.Indices.Add(BaseIndex);
            Data.Indices.Add(BaseIndex + 1);
            Data.Indices.Add(BaseIndex + 3);

            Data.Indices.Add(BaseIndex);
            Data.Indices.Add(BaseIndex + 3);
            Data.Indices.Add(BaseIndex + 2);
        }

        const auto AddCap = [&Data, Radius, HalfHeight](bool bTop)
        {
            const float Y = bTop ? HalfHeight : -HalfHeight;
            const uint32 CenterIndex = static_cast<uint32>(Data.Vertices.Add(MakeVertex(0.0f, Y, 0.0f, 0.5f, 0.5f)));
            const uint32 RingStart = static_cast<uint32>(Data.Vertices.Num());

            for (int32 Slice = 0; Slice <= NumSlices; ++Slice)
            {
                const float Theta = 2.0f * PI * static_cast<float>(Slice) / static_cast<float>(NumSlices);
                const float X = Radius * std::cos(Theta);
                const float Z = Radius * std::sin(Theta);

                Data.Vertices.Add(MakeVertex(X, Y, Z, X + 0.5f, Z + 0.5f));
            }

            for (int32 Slice = 0; Slice < NumSlices; ++Slice)
            {
                if (bTop)
                {
                    Data.Indices.Add(CenterIndex);
                    Data.Indices.Add(RingStart + static_cast<uint32>(Slice + 1));
                    Data.Indices.Add(RingStart + static_cast<uint32>(Slice));
                }
                else
                {
                    Data.Indices.Add(CenterIndex);
                    Data.Indices.Add(RingStart + static_cast<uint32>(Slice));
                    Data.Indices.Add(RingStart + static_cast<uint32>(Slice + 1));
                }
            }
        };

        AddCap(true);
        AddCap(false);

        return Data;
    }

    FStaticMeshBuildData CreateConeMeshData()
    {
        constexpr float Radius = 0.5f;
        constexpr float BottomY = -0.5f;
        constexpr float TopY = 0.5f;

        FStaticMeshBuildData Data;
        Data.Vertices.Reserve(NumSlices * 3 + NumSlices + 2);
        Data.Indices.Reserve(NumSlices * 6);

        for (int32 Slice = 0; Slice < NumSlices; ++Slice)
        {
            const float U0 = static_cast<float>(Slice) / static_cast<float>(NumSlices);
            const float U1 = static_cast<float>(Slice + 1) / static_cast<float>(NumSlices);
            const float Theta0 = 2.0f * PI * U0;
            const float Theta1 = 2.0f * PI * U1;

            const float X0 = Radius * std::cos(Theta0);
            const float Z0 = Radius * std::sin(Theta0);
            const float X1 = Radius * std::cos(Theta1);
            const float Z1 = Radius * std::sin(Theta1);

            const uint32 BaseIndex = static_cast<uint32>(Data.Vertices.Num());

            Data.Vertices.Add(MakeVertex(X0, BottomY, Z0, U0, 1.0f));
            Data.Vertices.Add(MakeVertex(0.0f, TopY, 0.0f, (U0 + U1) * 0.5f, 0.0f));
            Data.Vertices.Add(MakeVertex(X1, BottomY, Z1, U1, 1.0f));

            Data.Indices.Add(BaseIndex);
            Data.Indices.Add(BaseIndex + 1);
            Data.Indices.Add(BaseIndex + 2);
        }

        const uint32 CenterIndex = static_cast<uint32>(Data.Vertices.Add(MakeVertex(0.0f, BottomY, 0.0f, 0.5f, 0.5f)));
        const uint32 RingStart = static_cast<uint32>(Data.Vertices.Num());

        for (int32 Slice = 0; Slice <= NumSlices; ++Slice)
        {
            const float Theta = 2.0f * PI * static_cast<float>(Slice) / static_cast<float>(NumSlices);
            const float X = Radius * std::cos(Theta);
            const float Z = Radius * std::sin(Theta);

            Data.Vertices.Add(MakeVertex(X, BottomY, Z, X + 0.5f, Z + 0.5f));
        }

        for (int32 Slice = 0; Slice < NumSlices; ++Slice)
        {
            Data.Indices.Add(CenterIndex);
            Data.Indices.Add(RingStart + static_cast<uint32>(Slice));
            Data.Indices.Add(RingStart + static_cast<uint32>(Slice + 1));
        }

        return Data;
    }

    FStaticMeshBuildData CreateCapsuleMeshData()
    {
        constexpr float Radius = 0.35f;
        constexpr float HalfCylinderLength = 0.15f;
        constexpr int32 HemisphereSegments = 8;
        constexpr int32 RowSize = NumSlices + 1;

        struct FProfileRing
        {
            float Y;
            float Radius;
        };

        std::vector<FProfileRing> Rings;
        Rings.reserve(HemisphereSegments * 2 + 2);

        Rings.push_back({0.5f, 0.0f});

        for (int32 Segment = 1; Segment <= HemisphereSegments; ++Segment)
        {
            const float Phi = (PI * 0.5f) * static_cast<float>(Segment) / static_cast<float>(HemisphereSegments);
            Rings.push_back({HalfCylinderLength + Radius * std::cos(Phi), Radius * std::sin(Phi)});
        }

        Rings.push_back({-HalfCylinderLength, Radius});

        for (int32 Segment = 1; Segment <= HemisphereSegments; ++Segment)
        {
            const float Phi = (PI * 0.5f) * static_cast<float>(Segment) / static_cast<float>(HemisphereSegments);
            const float Angle = PI * 0.5f + Phi;

            Rings.push_back({-HalfCylinderLength + Radius * std::cos(Angle), Radius * std::sin(Angle)});
        }

        FStaticMeshBuildData Data;
        Data.Vertices.Reserve(static_cast<int32>(Rings.size()) * RowSize);
        Data.Indices.Reserve(static_cast<int32>(Rings.size() - 1) * NumSlices * 6);

        for (std::size_t Stack = 0; Stack < Rings.size(); ++Stack)
        {
            const FProfileRing& Ring = Rings[Stack];

            for (int32 Slice = 0; Slice <= NumSlices; ++Slice)
            {
                const float U = static_cast<float>(Slice) / static_cast<float>(NumSlices);
                const float Theta = 2.0f * PI * U;

                const float X = Ring.Radius * std::cos(Theta);
                const float Z = Ring.Radius * std::sin(Theta);
                const float V = 0.5f - Ring.Y;

                Data.Vertices.Add(MakeVertex(X, Ring.Y, Z, U, V));
            }
        }

        for (int32 Stack = 0; Stack < static_cast<int32>(Rings.size()) - 1; ++Stack)
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
                else if (Stack == static_cast<int32>(Rings.size()) - 2)
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
    UStaticMesh* Cube = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Cube, [&Renderer]() { return CreatePrimitiveMesh(Renderer, CreateCubeMeshData()); });
    UStaticMesh* Sphere = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Sphere, [&Renderer]() { return CreatePrimitiveMesh(Renderer, CreateSphereMeshData()); });
    UStaticMesh* Plane = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Plane, [&Renderer]() { return CreatePrimitiveMesh(Renderer, CreatePlaneMeshData()); });
    UStaticMesh* Cone = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Cone, [&Renderer]() { return CreatePrimitiveMesh(Renderer, CreateConeMeshData()); });
    UStaticMesh* Cylinder = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Cylinder, [&Renderer]() { return CreatePrimitiveMesh(Renderer, CreateCylinderMeshData()); });
    UStaticMesh* Capsule = AssetManager.Load<UStaticMesh>(BuiltinAssetPaths::Capsule, [&Renderer]() { return CreatePrimitiveMesh(Renderer, CreateCapsuleMeshData()); });

    return Cube != nullptr && Sphere != nullptr && Plane != nullptr && Cone != nullptr && Cylinder != nullptr && Capsule != nullptr;
}