#pragma once

#include "Renderer/Renderer.h"
#include "Core/PathUtils.h"
#include "Core/Logging/Log.h"
#include "Resources/AssetManager.h"
#include "Resources/Material.h"
#include "Resources/PrimitiveMeshAssets.h"
#include "Resources/StaticMesh.h"
#include "Resources/Texture2D.h"
#include "TextureAssetImporter.h"

#include "UObject/Object.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace OBJImporter
{
    inline constexpr std::uintmax_t MaxOBJFileBytes = 64ull * 1024ull * 1024ull;
    inline constexpr std::uintmax_t MaxMTLFileBytes = 16ull * 1024ull * 1024ull;
    inline constexpr std::uintmax_t MaxCookedCacheBytes = 512ull * 1024ull * 1024ull;

    inline constexpr std::size_t MaxOBJPositionCount = 1'000'000;
    inline constexpr std::size_t MaxOBJFaceVertexCount = 4096;
    inline constexpr uint32 MaxOBJOutputIndexCount = 3'000'000;
    inline constexpr uint32 MaxOBJMaterialCount = 4096;
    inline constexpr uint32 MaxOBJSectionCount = 65536;
    inline constexpr uint32 MaxCacheStringLength = 1024 * 1024;

    inline constexpr uint32 CookedCacheMagic = 0x4B4F424A;
    inline constexpr uint32 CookedCacheVersion = 1;

    static_assert(sizeof(FVertexSimple) == sizeof(float) * 12);
    static_assert(std::is_trivially_copyable_v<FVertexSimple>);

    inline std::string PathToUTF8(const std::filesystem::path& Path)
    {
        const std::u8string UTF8Path = Path.generic_u8string();
        return std::string(reinterpret_cast<const char*>(UTF8Path.data()), UTF8Path.size());
    }

    inline std::string Trim(const std::string& Text)
    {
        const std::size_t First = Text.find_first_not_of(" \t\r\n");

        if (First == std::string::npos)
        {
            return {};
        }

        const std::size_t Last = Text.find_last_not_of(" \t\r\n");
        return Text.substr(First, Last - First + 1);
    }

    inline std::string SanitizeFileName(const std::string& Name)
    {
        std::string Result;
        Result.reserve(Name.size());

        for (unsigned char Character : Name)
        {
            if ((Character >= 'a' && Character <= 'z') ||
                (Character >= 'A' && Character <= 'Z') ||
                (Character >= '0' && Character <= '9') ||
                Character == '_' ||
                Character == '-')
            {
                Result.push_back(static_cast<char>(Character));
            }
            else
            {
                Result.push_back('_');
            }
        }

        while (!Result.empty() && Result.front() == '_')
        {
            Result.erase(Result.begin());
        }

        while (!Result.empty() && Result.back() == '_')
        {
            Result.pop_back();
        }

        return Result.empty() ? "Unnamed" : Result;
    }

    inline bool HasOBJExtension(const std::filesystem::path& FilePath)
    {
        std::string Extension = PathToUTF8(FilePath.extension());

        std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });

        return Extension == ".obj";
    }

    inline bool IsValidContentPath(const std::filesystem::path& FilePath)
    {
        if (FilePath.empty() || FilePath.is_absolute())
        {
            return false;
        }

        const std::filesystem::path Normalized = FilePath.lexically_normal();

        for (const auto& Part : Normalized)
        {
            if (Part == "..")
            {
                return false;
            }
        }

        return PathToUTF8(Normalized).starts_with("Content/");
    }

    inline bool IsPathInsideDirectory(const std::filesystem::path& FilePath, const std::filesystem::path& Directory)
    {
        const std::filesystem::path Relative = FilePath.lexically_normal().lexically_relative(Directory.lexically_normal());

        if (Relative.empty() || Relative.is_absolute())
        {
            return false;
        }

        for (const auto& Part : Relative)
        {
            if (Part == "..")
            {
                return false;
            }
        }

        return true;
    }

    inline bool ResolveDependencyPath(const std::filesystem::path& BaseDirectory, const std::string& RelativeName, const std::filesystem::path& RootDirectory, std::filesystem::path& OutPath)
    {
        if (RelativeName.empty())
        {
            return false;
        }

        const std::filesystem::path RelativePath = FPathUtils::FromUTF8(RelativeName);

        if (RelativePath.empty() || RelativePath.is_absolute())
        {
            return false;
        }

        const std::filesystem::path Candidate = (BaseDirectory / RelativePath).lexically_normal();

        if (!IsPathInsideDirectory(Candidate, RootDirectory))
        {
            return false;
        }

        OutPath = Candidate;
        return true;
    }

    inline bool GetContentAssetPath(const std::filesystem::path& FullPath, FString& OutAssetPath)
    {
        std::error_code Error;
        const std::filesystem::path AbsolutePath = std::filesystem::absolute(FullPath, Error);

        if (Error)
        {
            return false;
        }

        const std::filesystem::path CurrentDirectory = std::filesystem::current_path(Error);

        if (Error)
        {
            return false;
        }

        const std::filesystem::path RelativePath = AbsolutePath.lexically_relative(CurrentDirectory);

        if (!IsValidContentPath(RelativePath))
        {
            return false;
        }

        OutAssetPath = FString(PathToUTF8(RelativePath));
        return true;
    }

    struct FOBJTexCoord
    {
        float U = 0.0f;
        float V = 0.0f;
    };

    struct FOBJFaceVertex
    {
        int32 PositionIndex = INDEX_NONE;
        int32 TexCoordIndex = INDEX_NONE;
        int32 NormalIndex = INDEX_NONE;
    };

    struct FObjMaterialInfo
    {
        std::string Name;
        std::filesystem::path MaterialLibraryPath;
        std::filesystem::path DiffuseTexturePath;

        FVector4 DiffuseColor = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
    };

    struct FObjRawFace
    {
        std::vector<FOBJFaceVertex> Vertices;
        std::string MaterialName;
    };

    struct FObjRawData
    {
        std::vector<FVector> Positions;
        std::vector<FOBJTexCoord> TexCoords;
        std::vector<FVector> Normals;

        std::vector<FObjRawFace> Faces;
        std::vector<FObjMaterialInfo> Materials;

        std::vector<std::filesystem::path> MaterialLibraries;
        std::vector<std::filesystem::path> DependencyFiles;
    };

    struct FObjCookedData
    {
        FStaticMeshBuildData MeshData;
    };

    inline bool ResolveOBJIndex(std::string_view Token, int32 ElementCount, int32& OutIndex)
    {
        if (Token.empty() || ElementCount <= 0)
        {
            return false;
        }

        int32 RawIndex = 0;

        const auto Result = std::from_chars(Token.data(), Token.data() + Token.size(), RawIndex);

        if (Result.ec != std::errc{} || Result.ptr != Token.data() + Token.size() || RawIndex == 0)
        {
            return false;
        }

        OutIndex = RawIndex > 0 ? RawIndex - 1 : ElementCount + RawIndex;

        return OutIndex >= 0 && OutIndex < ElementCount;
    }

    inline bool ResolveOBJFaceVertex(std::string_view Token, int32 PositionCount, int32 TexCoordCount, int32 NormalCount, FOBJFaceVertex& OutVertex)
    {
        const std::size_t FirstSlash = Token.find('/');
        const std::string_view PositionToken = FirstSlash == std::string_view::npos ? Token : Token.substr(0, FirstSlash);

        if (!ResolveOBJIndex(PositionToken, PositionCount, OutVertex.PositionIndex))
        {
            return false;
        }

        OutVertex.TexCoordIndex = INDEX_NONE;
        OutVertex.NormalIndex = INDEX_NONE;

        if (FirstSlash == std::string_view::npos)
        {
            return true;
        }

        const std::size_t SecondSlash = Token.find('/', FirstSlash + 1);
        const std::size_t TexCoordStart = FirstSlash + 1;
        const std::size_t TexCoordLength = SecondSlash == std::string_view::npos ? Token.size() - TexCoordStart : SecondSlash - TexCoordStart;
        const std::string_view TexCoordToken = Token.substr(TexCoordStart, TexCoordLength);

        if (!TexCoordToken.empty() && !ResolveOBJIndex(TexCoordToken, TexCoordCount, OutVertex.TexCoordIndex))
        {
            return false;
        }

        if (SecondSlash != std::string_view::npos)
        {
            const std::string_view NormalToken = Token.substr(SecondSlash + 1);

            if (!NormalToken.empty() && !ResolveOBJIndex(NormalToken, NormalCount, OutVertex.NormalIndex))
            {
                return false;
            }
        }

        return true;
    }

    inline std::string ParseQuotedOrToken(const std::string& Text)
    {
        std::istringstream Stream(Text);
        std::string Result;

        if (!(Stream >> std::quoted(Result)))
        {
            return {};
        }

        return Result;
    }

    inline bool ParseMTLFile(const std::filesystem::path& MTLPath, const std::filesystem::path& RootDirectory, FObjRawData& OutRawData)
    {
        std::error_code FileSystemError;
        const std::uintmax_t FileSize = std::filesystem::file_size(MTLPath, FileSystemError);

        if (FileSystemError || FileSize == 0 || FileSize > MaxMTLFileBytes)
        {
            return false;
        }

        std::ifstream InputFile(MTLPath);

        if (!InputFile.is_open())
        {
            return false;
        }

        FObjMaterialInfo* CurrentMaterial = nullptr;
        std::string Line;

        while (std::getline(InputFile, Line))
        {
            std::istringstream LineStream(Line);
            std::string Command;

            if (!(LineStream >> Command) || Command[0] == '#')
            {
                continue;
            }

            if (Command == "newmtl")
            {
                std::string MaterialName;
                std::getline(LineStream, MaterialName);
                MaterialName = Trim(MaterialName);

                if (MaterialName.empty())
                {
                    CurrentMaterial = nullptr;
                    continue;
                }

                if (OutRawData.Materials.size() >= MaxOBJMaterialCount)
                {
                    return false;
                }

                FObjMaterialInfo MaterialInfo;
                MaterialInfo.Name = MaterialName;
                MaterialInfo.MaterialLibraryPath = MTLPath;

                OutRawData.Materials.push_back(std::move(MaterialInfo));
                CurrentMaterial = &OutRawData.Materials.back();
            }
            else if (CurrentMaterial != nullptr && Command == "Kd")
            {
                float R = 1.0f;
                float G = 1.0f;
                float B = 1.0f;

                if (!(LineStream >> R >> G >> B) || !std::isfinite(R) || !std::isfinite(G) || !std::isfinite(B))
                {
                    continue;
                }

                CurrentMaterial->DiffuseColor.x = std::clamp(R, 0.0f, 1.0f);
                CurrentMaterial->DiffuseColor.y = std::clamp(G, 0.0f, 1.0f);
                CurrentMaterial->DiffuseColor.z = std::clamp(B, 0.0f, 1.0f);
            }
            else if (CurrentMaterial != nullptr && Command == "d")
            {
                float Alpha = 1.0f;

                if (LineStream >> Alpha && std::isfinite(Alpha))
                {
                    CurrentMaterial->DiffuseColor.w = std::clamp(Alpha, 0.0f, 1.0f);
                }
            }
            else if (CurrentMaterial != nullptr && Command == "Tr")
            {
                float Transparency = 0.0f;

                if (LineStream >> Transparency && std::isfinite(Transparency))
                {
                    CurrentMaterial->DiffuseColor.w = 1.0f - std::clamp(Transparency, 0.0f, 1.0f);
                }
            }
            else if (CurrentMaterial != nullptr && Command == "map_Kd")
            {
                std::string TextureName;
                std::getline(LineStream, TextureName);
                TextureName = Trim(TextureName);

                const std::string ParsedName = ParseQuotedOrToken(TextureName);

                if (!ParsedName.empty())
                {
                    std::filesystem::path TexturePath;

                    if (ResolveDependencyPath(MTLPath.parent_path(), ParsedName, RootDirectory, TexturePath))
                    {
                        CurrentMaterial->DiffuseTexturePath = TexturePath;
                        OutRawData.DependencyFiles.push_back(TexturePath);
                    }
                }
            }
        }

        return !InputFile.bad();
    }

    inline bool ParseRawOBJ(const std::filesystem::path& FilePath, FObjRawData& OutRawData)
    {
        OutRawData = {};

        std::error_code FileSystemError;
        const std::uintmax_t FileSize = std::filesystem::file_size(FilePath, FileSystemError);

        if (FileSystemError || FileSize == 0 || FileSize > MaxOBJFileBytes)
        {
            return false;
        }

        const std::filesystem::path AbsolutePath = std::filesystem::absolute(FilePath, FileSystemError);

        if (FileSystemError)
        {
            return false;
        }

        const std::filesystem::path RootDirectory = AbsolutePath.parent_path().lexically_normal();

        std::ifstream InputFile(AbsolutePath);

        if (!InputFile.is_open())
        {
            return false;
        }

        std::vector<std::string> MaterialLibraryNames;
        std::string CurrentMaterialName;
        std::string Line;

        while (std::getline(InputFile, Line))
        {
            std::istringstream LineStream(Line);
            std::string Command;

            if (!(LineStream >> Command) || Command[0] == '#')
            {
                continue;
            }

            if (Command == "v")
            {
                if (OutRawData.Positions.size() >= MaxOBJPositionCount)
                {
                    return false;
                }

                float X = 0.0f;
                float Y = 0.0f;
                float Z = 0.0f;

                if (!(LineStream >> X >> Y >> Z) || !std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z))
                {
                    return false;
                }

                OutRawData.Positions.emplace_back(X, Y, Z);
            }
            else if (Command == "vt")
            {
                if (OutRawData.TexCoords.size() >= MaxOBJPositionCount)
                {
                    return false;
                }

                FOBJTexCoord TexCoord;

                if (!(LineStream >> TexCoord.U))
                {
                    return false;
                }

                if (!(LineStream >> TexCoord.V))
                {
                    TexCoord.V = 0.0f;
                }

                if (!std::isfinite(TexCoord.U) || !std::isfinite(TexCoord.V))
                {
                    return false;
                }

                OutRawData.TexCoords.push_back(TexCoord);
            }
            else if (Command == "vn")
            {
                if (OutRawData.Normals.size() >= MaxOBJPositionCount)
                {
                    return false;
                }

                float X = 0.0f;
                float Y = 0.0f;
                float Z = 0.0f;

                if (!(LineStream >> X >> Y >> Z) || !std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z))
                {
                    return false;
                }

                OutRawData.Normals.emplace_back(X, Y, Z);
            }
            else if (Command == "mtllib")
            {
                std::string MaterialLibraryName;

                while (LineStream >> std::quoted(MaterialLibraryName))
                {
                    std::filesystem::path MaterialLibraryPath;

                    if (ResolveDependencyPath(RootDirectory, MaterialLibraryName, RootDirectory, MaterialLibraryPath))
                    {
                        OutRawData.MaterialLibraries.push_back(MaterialLibraryPath);
                        OutRawData.DependencyFiles.push_back(MaterialLibraryPath);
                    }
                }
            }
            else if (Command == "usemtl")
            {
                std::getline(LineStream, CurrentMaterialName);
                CurrentMaterialName = Trim(CurrentMaterialName);
            }
            else if (Command == "f")
            {
                if (OutRawData.Faces.size() >= MaxOBJOutputIndexCount)
                {
                    return false;
                }

                FObjRawFace Face;
                Face.MaterialName = CurrentMaterialName;

                std::string Token;

                while (LineStream >> Token)
                {
                    if (!Token.empty() && Token[0] == '#')
                    {
                        break;
                    }

                    if (Face.Vertices.size() >= MaxOBJFaceVertexCount)
                    {
                        return false;
                    }

                    FOBJFaceVertex FaceVertex;

                    if (!ResolveOBJFaceVertex(Token, static_cast<int32>(OutRawData.Positions.size()), static_cast<int32>(OutRawData.TexCoords.size()), static_cast<int32>(OutRawData.Normals.size()), FaceVertex))
                    {
                        return false;
                    }

                    Face.Vertices.push_back(FaceVertex);
                }

                if (Face.Vertices.size() < 3)
                {
                    return false;
                }

                OutRawData.Faces.push_back(std::move(Face));
            }
        }

        if (InputFile.bad() || OutRawData.Positions.size() < 3 || OutRawData.Faces.empty())
        {
            return false;
        }

        for (const std::filesystem::path& MaterialLibraryPath : OutRawData.MaterialLibraries)
        {
            std::error_code MTLFileError;

            if (std::filesystem::is_regular_file(MaterialLibraryPath, MTLFileError) && !MTLFileError)
            {
                if (!ParseMTLFile(MaterialLibraryPath, RootDirectory, OutRawData))
                {
                    return false;
                }
            }
        }

        return true;
    }

    inline const FObjMaterialInfo* FindMaterialInfo(const FObjRawData& RawData, const std::string& MaterialName)
    {
        for (const FObjMaterialInfo& MaterialInfo : RawData.Materials)
        {
            if (MaterialInfo.Name == MaterialName)
            {
                return &MaterialInfo;
            }
        }

        return nullptr;
    }

    inline FString MakeMaterialAssetPath(const std::filesystem::path& OBJPath, const std::string& MaterialName)
    {
        const std::string OBJStem = SanitizeFileName(PathToUTF8(OBJPath.stem()));
        const std::string SafeMaterialName = SanitizeFileName(MaterialName);

        return FString("Content/Materials/OBJ_" + OBJStem + "_" + SafeMaterialName + ".kmat");
    }

    inline UMaterial* LoadOrCreateMaterialAsset(const FObjMaterialInfo& MaterialInfo, const std::filesystem::path& OBJPath, FAssetManager& AssetManager, URenderer& Renderer, FString& OutAssetPath)
    {
        OutAssetPath = MakeMaterialAssetPath(OBJPath, MaterialInfo.Name);

        if (UMaterial* LoadedMaterial = AssetManager.Find<UMaterial>(OutAssetPath))
        {
            return LoadedMaterial;
        }

        std::filesystem::path MaterialRelativePath = FPathUtils::FromUTF8(OutAssetPath.ToStd());
        const std::filesystem::path MaterialFullPath = std::filesystem::current_path() / MaterialRelativePath;
        std::error_code FileSystemError;

        if (std::filesystem::is_regular_file(MaterialFullPath, FileSystemError) && !FileSystemError)
        {
            return TextureImporter::LoadMaterialAsset(OutAssetPath, AssetManager, Renderer);
        }

        UTexture2D* DiffuseTexture = nullptr;

        if (!MaterialInfo.DiffuseTexturePath.empty())
        {
            FString TextureAssetPath;

            if (GetContentAssetPath(MaterialInfo.DiffuseTexturePath, TextureAssetPath))
            {
                DiffuseTexture = TextureImporter::LoadTextureAsset(TextureAssetPath, AssetManager, Renderer);
            }
        }

        UMaterial* Material = AssetManager.Load<UMaterial>(OutAssetPath, [&]() -> UMaterial*
        {
            UMaterial* NewMaterial = NewObject<UMaterial>();

            if (NewMaterial == nullptr)
            {
                return nullptr;
            }

            NewMaterial->SetBaseColor(MaterialInfo.DiffuseColor);
            NewMaterial->SetBaseColorTexture(DiffuseTexture);
            return NewMaterial;
        });

        if (Material != nullptr)
        {
            if (!TextureImporter::SaveMaterialAsset(Material))
            {
                UE_LOG(Error, "[OBJ Import] Failed to save material: %s", OutAssetPath.ToStd().c_str());
            }

            AssetManager.RefreshAssetRegistry();
        }

        return Material;
    }

    struct FObjVertexKey
    {
        int32 PositionIndex = INDEX_NONE;
        int32 TexCoordIndex = INDEX_NONE;
        int32 NormalIndex = INDEX_NONE;
        int32 FaceIndex = INDEX_NONE;

        bool operator==(const FObjVertexKey& Other) const
        {
            return PositionIndex == Other.PositionIndex &&
                TexCoordIndex == Other.TexCoordIndex &&
                NormalIndex == Other.NormalIndex &&
                FaceIndex == Other.FaceIndex;
        }
    };

    struct FObjVertexKeyHash
    {
        std::size_t operator()(const FObjVertexKey& Key) const
        {
            std::size_t Hash = std::hash<int32>{}(Key.PositionIndex);
            Hash ^= std::hash<int32>{}(Key.TexCoordIndex) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
            Hash ^= std::hash<int32>{}(Key.NormalIndex) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
            Hash ^= std::hash<int32>{}(Key.FaceIndex) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
            return Hash;
        }
    };

    inline bool BuildCookedGeometry(const FObjRawData& RawData, const std::filesystem::path& OBJPath, FAssetManager& AssetManager, URenderer& Renderer, FObjCookedData& OutCookedData)
    {
        OutCookedData.MeshData = {};

        if (RawData.Positions.size() < 3 || RawData.Faces.empty())
        {
            return false;
        }

        FStaticMeshBuildData& MeshData = OutCookedData.MeshData;

        MeshData.Materials.Add(nullptr);
        MeshData.MaterialAssetPaths.Add(FString());

        std::unordered_map<std::string, uint32> MaterialSlots;
        MaterialSlots.emplace("", 0u);

        for (const FObjRawFace& Face : RawData.Faces)
        {
            if (Face.MaterialName.empty() || MaterialSlots.find(Face.MaterialName) != MaterialSlots.end())
            {
                continue;
            }

            const FObjMaterialInfo* MaterialInfo = FindMaterialInfo(RawData, Face.MaterialName);

            if (MaterialInfo == nullptr)
            {
                MaterialSlots.emplace(Face.MaterialName, 0u);
                continue;
            }

            FString MaterialAssetPath;
            UMaterial* Material = LoadOrCreateMaterialAsset(*MaterialInfo, OBJPath, AssetManager, Renderer, MaterialAssetPath);

            if (MeshData.Materials.Num() >= static_cast<int32>(MaxOBJMaterialCount))
            {
                return false;
            }

            const uint32 MaterialIndex = static_cast<uint32>(MeshData.Materials.Add(Material));
            MeshData.MaterialAssetPaths.Add(MaterialAssetPath);
            MaterialSlots.emplace(Face.MaterialName, MaterialIndex);
        }

        std::unordered_map<FObjVertexKey, uint32, FObjVertexKeyHash> VertexMap;
        VertexMap.reserve((std::min)(RawData.Positions.size() * 2, static_cast<std::size_t>(MaxOBJOutputIndexCount)));

        uint32 CurrentMaterialIndex = (std::numeric_limits<uint32>::max)();
        uint32 FaceIndex = 0;

        for (const FObjRawFace& Face : RawData.Faces)
        {
            
            const auto MaterialIterator = MaterialSlots.find(Face.MaterialName);
            const uint32 FaceMaterialIndex = MaterialIterator != MaterialSlots.end() ? MaterialIterator->second : 0u;

            const FVector& P0 = RawData.Positions[Face.Vertices[0].PositionIndex];
            FVector FaceNormal(0.0f, 0.0f, 0.0f);
            bool bHasValidFaceNormal = false;

            for (std::size_t TriangleIndex = 1; TriangleIndex + 1 < Face.Vertices.size(); ++TriangleIndex)
            {
                const FVector& P1 = RawData.Positions[Face.Vertices[TriangleIndex].PositionIndex];
                const FVector& P2 = RawData.Positions[Face.Vertices[TriangleIndex + 1].PositionIndex];

                const FVector EdgeA = P1 - P0;
                const FVector EdgeB = P2 - P0;
                FVector CandidateNormal;
                EdgeA.Cross(EdgeB, CandidateNormal);

                const float EdgeScaleSquared = EdgeA.LengthSquared() * EdgeB.LengthSquared();

                if (EdgeScaleSquared > std::numeric_limits<float>::min() && CandidateNormal.LengthSquared() > EdgeScaleSquared * 1.0e-12f)
                {
                    FaceNormal = CandidateNormal;
                    bHasValidFaceNormal = true;
                    break;
                }
            }

            if (!bHasValidFaceNormal)
            {
                UE_LOG(Warning, "[OBJ Import] Skipping degenerate face: Index=%u, VertexCount=%zu", FaceIndex, Face.Vertices.size());
                ++FaceIndex;
                continue;
            }

            FaceNormal.Normalize();

            if (MeshData.Sections.IsEmpty() || CurrentMaterialIndex != FaceMaterialIndex)
            {
                FStaticMeshSection Section;
                Section.FirstIndex = static_cast<uint32>(MeshData.Indices.Num());
                Section.IndexCount = 0;
                Section.MaterialIndex = FaceMaterialIndex;
                Section.MaterialName = FString(Face.MaterialName);

                MeshData.Sections.Add(Section);
                CurrentMaterialIndex = FaceMaterialIndex;
            }

            std::vector<uint32> FaceIndices;
            FaceIndices.reserve(Face.Vertices.size());

            for (const FOBJFaceVertex& FaceVertex : Face.Vertices)
            {
                FObjVertexKey Key;
                Key.PositionIndex = FaceVertex.PositionIndex;
                Key.TexCoordIndex = FaceVertex.TexCoordIndex;
                Key.NormalIndex = FaceVertex.NormalIndex;
                Key.FaceIndex = FaceVertex.NormalIndex == INDEX_NONE ? static_cast<int32>(FaceIndex) : INDEX_NONE;

                const auto ExistingVertex = VertexMap.find(Key);

                if (ExistingVertex != VertexMap.end())
                {
                    FaceIndices.push_back(ExistingVertex->second);
                    continue;
                }

                if (MeshData.Vertices.Num() >= static_cast<int32>(MaxOBJOutputIndexCount))
                {
                    return false;
                }

                const FVector& Position = RawData.Positions[FaceVertex.PositionIndex];

                FOBJTexCoord TexCoord;

                if (FaceVertex.TexCoordIndex != INDEX_NONE)
                {
                    TexCoord = RawData.TexCoords[FaceVertex.TexCoordIndex];
                }

                FVector Normal = FaceNormal;

                if (FaceVertex.NormalIndex != INDEX_NONE)
                {
                    Normal = RawData.Normals[FaceVertex.NormalIndex];

                    if (Normal.LengthSquared() > 1.0e-8f)
                    {
                        Normal.Normalize();
                    }
                    else
                    {
                        Normal = FaceNormal;
                    }
                }

                FVertexSimple Vertex = {
                    Position.x, Position.y, Position.z,
                    1.0f, 1.0f, 1.0f, 1.0f,
                    TexCoord.U, 1.0f - TexCoord.V,
                    Normal.x, Normal.y, Normal.z
                };

                const uint32 NewVertexIndex = static_cast<uint32>(MeshData.Vertices.Add(Vertex));

                VertexMap.emplace(Key, NewVertexIndex);
                FaceIndices.push_back(NewVertexIndex);
            }
            
            uint32 AddedIndexCount = 0;
            const FVector& TriangleP0 = RawData.Positions[Face.Vertices[0].PositionIndex];

            for (std::size_t Index = 1; Index + 1 < FaceIndices.size(); ++Index)
            {
                const FVector& TriangleP1 = RawData.Positions[Face.Vertices[Index].PositionIndex];
                const FVector& TriangleP2 = RawData.Positions[Face.Vertices[Index + 1].PositionIndex];

                const FVector EdgeA = TriangleP1 - TriangleP0;
                const FVector EdgeB = TriangleP2 - TriangleP0;
                FVector TriangleNormal;
                EdgeA.Cross(EdgeB, TriangleNormal);

                const float EdgeScaleSquared = EdgeA.LengthSquared() * EdgeB.LengthSquared();

                if (EdgeScaleSquared <= std::numeric_limits<float>::min() || TriangleNormal.LengthSquared() <= EdgeScaleSquared * 1.0e-12f)
                {
                    continue;
                }

                if (MeshData.Indices.Num() > static_cast<int32>(MaxOBJOutputIndexCount) - 3)
                {
                    return false;
                }

                MeshData.Indices.Add(FaceIndices[0]);
                MeshData.Indices.Add(FaceIndices[Index]);
                MeshData.Indices.Add(FaceIndices[Index + 1]);
                AddedIndexCount += 3u;
            }

            FStaticMeshSection& Section = MeshData.Sections[MeshData.Sections.Num() - 1];
            Section.IndexCount += AddedIndexCount;

            ++FaceIndex;
        }

        MeshData.bHasVertexNormals = true;

        return !MeshData.Vertices.IsEmpty() &&
            MeshData.Indices.Num() >= 3 &&
            MeshData.Indices.Num() % 3 == 0 &&
            !MeshData.Sections.IsEmpty();
    }

    class FWindowsBinWriter
    {
    public:
        explicit FWindowsBinWriter(const std::filesystem::path& FilePath)
            : Stream(FilePath, std::ios::binary | std::ios::trunc)
        {
        }

        bool IsOpen() const
        {
            return Stream.is_open();
        }

        bool WriteBytes(const void* Data, std::size_t ByteCount)
        {
            if (!Stream.is_open() || (Data == nullptr && ByteCount > 0))
            {
                return false;
            }

            Stream.write(static_cast<const char*>(Data), static_cast<std::streamsize>(ByteCount));
            return Stream.good();
        }

        template<typename TValue>
        bool WriteValue(const TValue& Value)
        {
            static_assert(std::is_trivially_copyable_v<TValue>);
            return WriteBytes(&Value, sizeof(TValue));
        }

        bool WriteString(const std::string& Value)
        {
            if (Value.size() > MaxCacheStringLength)
            {
                return false;
            }

            const uint32 Length = static_cast<uint32>(Value.size());

            return WriteValue(Length) && WriteBytes(Value.data(), Length);
        }

        bool Flush()
        {
            if (!Stream.is_open())
            {
                return false;
            }

            Stream.flush();
            return Stream.good();
        }

        void Close()
        {
            Stream.close();
        }

    private:
        std::ofstream Stream;
    };

    class FWindowsBinReader
    {
    public:
        explicit FWindowsBinReader(const std::filesystem::path& FilePath)
            : Stream(FilePath, std::ios::binary)
        {
        }

        bool IsOpen() const
        {
            return Stream.is_open();
        }

        bool ReadBytes(void* Destination, std::size_t ByteCount)
        {
            if (!Stream.is_open() || (Destination == nullptr && ByteCount > 0))
            {
                return false;
            }

            Stream.read(static_cast<char*>(Destination), static_cast<std::streamsize>(ByteCount));
            return Stream.good();
        }

        template<typename TValue>
        bool ReadValue(TValue& OutValue)
        {
            static_assert(std::is_trivially_copyable_v<TValue>);
            return ReadBytes(&OutValue, sizeof(TValue));
        }

        bool ReadString(std::string& OutValue)
        {
            uint32 Length = 0;

            if (!ReadValue(Length) || Length > MaxCacheStringLength)
            {
                return false;
            }

            OutValue.resize(Length);

            return Length == 0 || ReadBytes(OutValue.data(), Length);
        }

    private:
        std::ifstream Stream;
    };

    struct FObjSourceSignature
    {
        uint64 FileSize = 0;
        int64 LastWriteTime = 0;
    };

    inline bool GetSourceSignature(const std::filesystem::path& SourcePath, FObjSourceSignature& OutSignature)
    {
        std::error_code FileSystemError;

        const std::uintmax_t FileSize = std::filesystem::file_size(SourcePath, FileSystemError);

        if (FileSystemError || FileSize == 0 || FileSize > MaxOBJFileBytes)
        {
            return false;
        }

        const std::filesystem::file_time_type LastWriteTime = std::filesystem::last_write_time(SourcePath, FileSystemError);

        if (FileSystemError)
        {
            return false;
        }

        OutSignature.FileSize = static_cast<uint64>(FileSize);
        OutSignature.LastWriteTime = static_cast<int64>(LastWriteTime.time_since_epoch().count());

        return true;
    }

    inline std::filesystem::path GetCookedCachePath(const std::filesystem::path& SourcePath)
    {
        std::filesystem::path CachePath = SourcePath;
        CachePath += ".bin";
        return CachePath;
    }

    inline bool ValidateCookedData(const FStaticMeshBuildData& MeshData)
    {
        if (MeshData.Vertices.Num() < 3 || MeshData.Indices.Num() < 3 || MeshData.Indices.Num() % 3 != 0)
        {
            return false;
        }

        if (MeshData.Materials.Num() != MeshData.MaterialAssetPaths.Num())
        {
            return false;
        }

        if (MeshData.Materials.Num() <= 0 || MeshData.Materials.Num() > static_cast<int32>(MaxOBJMaterialCount))
        {
            return false;
        }

        for (uint32 Index : MeshData.Indices)
        {
            if (Index >= static_cast<uint32>(MeshData.Vertices.Num()))
            {
                return false;
            }
        }

        uint32 ExpectedFirstIndex = 0;

        for (const FStaticMeshSection& Section : MeshData.Sections)
        {
            if (Section.IndexCount == 0 || Section.IndexCount % 3 != 0 || Section.FirstIndex != ExpectedFirstIndex || Section.MaterialIndex >= static_cast<uint32>(MeshData.Materials.Num()))
            {
                return false;
            }

            if (Section.IndexCount > static_cast<uint32>(MeshData.Indices.Num()) - ExpectedFirstIndex)
            {
                return false;
            }

            ExpectedFirstIndex += Section.IndexCount;
        }

        return !MeshData.Sections.IsEmpty() && ExpectedFirstIndex == static_cast<uint32>(MeshData.Indices.Num());
    }

    inline bool SaveCookedCache(const std::filesystem::path& SourcePath, const FStaticMeshBuildData& MeshData)
    {
        if (!ValidateCookedData(MeshData))
        {
            return false;
        }

        FObjSourceSignature Signature;

        if (!GetSourceSignature(SourcePath, Signature))
        {
            return false;
        }

        const std::filesystem::path CachePath = GetCookedCachePath(SourcePath);
        std::filesystem::path TemporaryPath = CachePath;
        TemporaryPath += ".tmp";

        std::error_code FileSystemError;
        std::filesystem::create_directories(CachePath.parent_path(), FileSystemError);

        if (FileSystemError)
        {
            return false;
        }

        FWindowsBinWriter Writer(TemporaryPath);

        if (!Writer.IsOpen())
        {
            return false;
        }

        const uint32 VertexCount = static_cast<uint32>(MeshData.Vertices.Num());
        const uint32 IndexCount = static_cast<uint32>(MeshData.Indices.Num());
        const uint32 SectionCount = static_cast<uint32>(MeshData.Sections.Num());
        const uint32 MaterialCount = static_cast<uint32>(MeshData.MaterialAssetPaths.Num());
        const uint32 HasNormals = MeshData.bHasVertexNormals ? 1u : 0u;

        bool bSuccess =
            Writer.WriteValue(CookedCacheMagic) &&
            Writer.WriteValue(CookedCacheVersion) &&
            Writer.WriteValue(Signature.FileSize) &&
            Writer.WriteValue(Signature.LastWriteTime) &&
            Writer.WriteValue(VertexCount) &&
            Writer.WriteValue(IndexCount) &&
            Writer.WriteValue(SectionCount) &&
            Writer.WriteValue(MaterialCount) &&
            Writer.WriteValue(HasNormals);

        if (bSuccess)
        {
            bSuccess = Writer.WriteBytes(MeshData.Vertices.GetData(), sizeof(FVertexSimple) * VertexCount);
        }

        if (bSuccess)
        {
            bSuccess = Writer.WriteBytes(MeshData.Indices.GetData(), sizeof(uint32) * IndexCount);
        }

        for (const FStaticMeshSection& Section : MeshData.Sections)
        {
            if (!bSuccess)
            {
                break;
            }

            bSuccess =
                Writer.WriteValue(Section.FirstIndex) &&
                Writer.WriteValue(Section.IndexCount) &&
                Writer.WriteValue(Section.MaterialIndex) &&
                Writer.WriteString(Section.MaterialName.ToStd());
        }

        for (const FString& MaterialPath : MeshData.MaterialAssetPaths)
        {
            if (!bSuccess)
            {
                break;
            }

            bSuccess = Writer.WriteString(MaterialPath.ToStd());
        }

        bSuccess = bSuccess && Writer.Flush();
        Writer.Close();

        if (!bSuccess)
        {
            std::filesystem::remove(TemporaryPath, FileSystemError);
            return false;
        }

        if (!MoveFileExW(TemporaryPath.c_str(), CachePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(TemporaryPath, FileSystemError);
            return false;
        }

        return true;
    }

    inline bool LoadCookedCache(const std::filesystem::path& SourcePath, FAssetManager& AssetManager, URenderer& Renderer, FStaticMeshBuildData& OutMeshData)
    {
        const std::filesystem::path CachePath = GetCookedCachePath(SourcePath);

        std::error_code FileSystemError;
        const std::uintmax_t CacheFileSize = std::filesystem::file_size(CachePath, FileSystemError);

        if (FileSystemError || CacheFileSize == 0 || CacheFileSize > MaxCookedCacheBytes)
        {
            return false;
        }

        FObjSourceSignature CurrentSignature;

        if (!GetSourceSignature(SourcePath, CurrentSignature))
        {
            return false;
        }

        FWindowsBinReader Reader(CachePath);

        if (!Reader.IsOpen())
        {
            return false;
        }

        uint32 Magic = 0;
        uint32 Version = 0;
        uint64 SavedFileSize = 0;
        int64 SavedLastWriteTime = 0;

        uint32 VertexCount = 0;
        uint32 IndexCount = 0;
        uint32 SectionCount = 0;
        uint32 MaterialCount = 0;
        uint32 HasNormals = 0;

        if (!Reader.ReadValue(Magic) ||
            !Reader.ReadValue(Version) ||
            !Reader.ReadValue(SavedFileSize) ||
            !Reader.ReadValue(SavedLastWriteTime) ||
            !Reader.ReadValue(VertexCount) ||
            !Reader.ReadValue(IndexCount) ||
            !Reader.ReadValue(SectionCount) ||
            !Reader.ReadValue(MaterialCount) ||
            !Reader.ReadValue(HasNormals))
        {
            return false;
        }

        if (Magic != CookedCacheMagic ||
            Version != CookedCacheVersion ||
            SavedFileSize != CurrentSignature.FileSize ||
            SavedLastWriteTime != CurrentSignature.LastWriteTime ||
            VertexCount < 3 ||
            VertexCount > MaxOBJOutputIndexCount ||
            IndexCount < 3 ||
            IndexCount > MaxOBJOutputIndexCount ||
            IndexCount % 3 != 0 ||
            SectionCount == 0 ||
            SectionCount > MaxOBJSectionCount ||
            MaterialCount == 0 ||
            MaterialCount > MaxOBJMaterialCount ||
            HasNormals > 1)
        {
            return false;
        }

        FStaticMeshBuildData LoadedData;

        std::vector<FVertexSimple> Vertices(VertexCount);
        std::vector<uint32> Indices(IndexCount);

        if (!Reader.ReadBytes(Vertices.data(), sizeof(FVertexSimple) * VertexCount) ||
            !Reader.ReadBytes(Indices.data(), sizeof(uint32) * IndexCount))
        {
            return false;
        }

        for (const FVertexSimple& Vertex : Vertices)
        {
            if (!std::isfinite(Vertex.x) || !std::isfinite(Vertex.y) || !std::isfinite(Vertex.z) ||
                !std::isfinite(Vertex.r) || !std::isfinite(Vertex.g) || !std::isfinite(Vertex.b) || !std::isfinite(Vertex.a) ||
                !std::isfinite(Vertex.u) || !std::isfinite(Vertex.v) ||
                !std::isfinite(Vertex.nx) || !std::isfinite(Vertex.ny) || !std::isfinite(Vertex.nz))
            {
                return false;
            }

            LoadedData.Vertices.Add(Vertex);
        }

        for (uint32 Index : Indices)
        {
            if (Index >= VertexCount)
            {
                return false;
            }

            LoadedData.Indices.Add(Index);
        }

        for (uint32 Index = 0; Index < SectionCount; ++Index)
        {
            FStaticMeshSection Section;
            std::string MaterialName;

            if (!Reader.ReadValue(Section.FirstIndex) ||
                !Reader.ReadValue(Section.IndexCount) ||
                !Reader.ReadValue(Section.MaterialIndex) ||
                !Reader.ReadString(MaterialName))
            {
                return false;
            }

            Section.MaterialName = FString(MaterialName);
            LoadedData.Sections.Add(Section);
        }

        for (uint32 Index = 0; Index < MaterialCount; ++Index)
        {
            std::string MaterialPath;

            if (!Reader.ReadString(MaterialPath))
            {
                return false;
            }

            FString AssetPath(MaterialPath);
            UMaterial* Material = nullptr;

            if (!MaterialPath.empty())
            {
                Material = TextureImporter::LoadMaterialAsset(AssetPath, AssetManager, Renderer);

                if (Material == nullptr)
                {
                    return false;
                }
            }

            LoadedData.Materials.Add(Material);
            LoadedData.MaterialAssetPaths.Add(AssetPath);
        }

        LoadedData.bHasVertexNormals = HasNormals != 0;

        if (!ValidateCookedData(LoadedData))
        {
            return false;
        }

        OutMeshData = std::move(LoadedData);
        return true;
    }

    inline bool BuildOBJ(const std::filesystem::path& OBJPath, FAssetManager& AssetManager, URenderer& Renderer, FObjCookedData& OutCookedData)
    {
        FObjRawData RawData;

        if (!ParseRawOBJ(OBJPath, RawData))
        {
            UE_LOG(Error, "[OBJ Import] BuildOBJ: ParseRawOBJ failed: %s", PathToUTF8(OBJPath).c_str());
            return false;
        }

        if (!BuildCookedGeometry(RawData, OBJPath, AssetManager, Renderer, OutCookedData))
        {
            UE_LOG(Error, "[OBJ Import] BuildOBJ: BuildCookedGeometry failed: %s", PathToUTF8(OBJPath).c_str());
            return false;
        }

        if (!ValidateCookedData(OutCookedData.MeshData))
        {
            const FStaticMeshBuildData& MeshData = OutCookedData.MeshData;
            UE_LOG(Error, "[OBJ Import] ValidateCookedData failed: Vertices=%d, Indices=%d, Sections=%d, Materials=%d, MaterialPaths=%d", MeshData.Vertices.Num(), MeshData.Indices.Num(), MeshData.Sections.Num(), MeshData.Materials.Num(), MeshData.MaterialAssetPaths.Num());
            return false;
        }

        UE_LOG(Info, "[OBJ Import] Geometry built: Vertices=%d, Indices=%d, Sections=%d", OutCookedData.MeshData.Vertices.Num(), OutCookedData.MeshData.Indices.Num(), OutCookedData.MeshData.Sections.Num());
        return true;
    }

    inline bool ResolveOBJAssetPath(const FString& AssetPath, std::filesystem::path& OutRelativePath, std::filesystem::path& OutFullPath)
    {
        if (AssetPath.IsEmpty())
        {
            return false;
        }

        OutRelativePath = FPathUtils::FromUTF8(AssetPath.ToStd()).lexically_normal();

        if (!IsValidContentPath(OutRelativePath) || !HasOBJExtension(OutRelativePath))
        {
            return false;
        }

        OutFullPath = std::filesystem::current_path() / OutRelativePath;

        std::error_code FileSystemError;

        if (!std::filesystem::is_regular_file(OutFullPath, FileSystemError) || FileSystemError)
        {
            return false;
        }

        return true;
    }

    inline UStaticMesh* LoadOBJAsset(const FString& AssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (UStaticMesh* ExistingMesh = AssetManager.Find<UStaticMesh>(AssetPath))
        {
            return ExistingMesh;
        }

        std::filesystem::path RelativePath;
        std::filesystem::path FullPath;

        if (!ResolveOBJAssetPath(AssetPath, RelativePath, FullPath))
        {
            return nullptr;
        }

        return AssetManager.Load<UStaticMesh>(AssetPath, [&]() -> UStaticMesh*
        {
            FStaticMeshBuildData MeshData;

            if (!LoadCookedCache(FullPath, AssetManager, Renderer, MeshData))
            {
                FObjCookedData CookedData;

                if (!BuildOBJ(FullPath, AssetManager, Renderer, CookedData))
                {
                    return nullptr;
                }

                MeshData = std::move(CookedData.MeshData);
                SaveCookedCache(FullPath, MeshData);
            }

            UStaticMesh* NewMesh = NewObject<UStaticMesh>();

            if (NewMesh == nullptr)
            {
                return nullptr;
            }

            if (!NewMesh->Initialize(Renderer, std::move(MeshData)))
            {
                UE_LOG(Error, "[OBJ Import] UStaticMesh::Initialize failed: %s", AssetPath.ToStd().c_str());
                delete NewMesh;
                return nullptr;
            }

            return NewMesh;
        });
    }

    inline bool ReimportOBJAsset(UStaticMesh* Mesh, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (Mesh == nullptr)
        {
            return false;
        }

        std::filesystem::path RelativePath;
        std::filesystem::path FullPath;

        if (!ResolveOBJAssetPath(Mesh->GetAssetPath(), RelativePath, FullPath))
        {
            return false;
        }

        FObjCookedData CookedData;

        if (!BuildOBJ(FullPath, AssetManager, Renderer, CookedData))
        {
            return false;
        }

        FStaticMeshBuildData CacheData = CookedData.MeshData;

        if (!Mesh->Reinitialize(Renderer, std::move(CookedData.MeshData)))
        {
            return false;
        }

        SaveCookedCache(FullPath, CacheData);
        return true;
    }

    inline void RegisterImportedStaticMeshes(FAssetManager& AssetManager, URenderer& Renderer)
    {
        const std::filesystem::path ContentDirectory = std::filesystem::current_path() / "Content";
        std::error_code DirectoryError;

        if (!std::filesystem::is_directory(ContentDirectory, DirectoryError) || DirectoryError)
        {
            return;
        }

        for (std::filesystem::recursive_directory_iterator It(ContentDirectory, std::filesystem::directory_options::skip_permission_denied, DirectoryError), End; !DirectoryError && It != End; It.increment(DirectoryError))
        {
            std::error_code EntryError;

            if (!It->is_regular_file(EntryError) || EntryError || !HasOBJExtension(It->path()))
            {
                continue;
            }

            const FString AssetPath(PathToUTF8(It->path().lexically_relative(std::filesystem::current_path())));
            LoadOBJAsset(AssetPath, AssetManager, Renderer);
        }
    }

    inline bool CopyDependencyFile(const std::filesystem::path& SourceFile, const std::filesystem::path& SourceRoot, const std::filesystem::path& DestinationRoot)
    {
        std::error_code FileSystemError;

        if (!std::filesystem::is_regular_file(SourceFile, FileSystemError) || FileSystemError)
        {
            return true;
        }

        const std::filesystem::path RelativePath = SourceFile.lexically_normal().lexically_relative(SourceRoot.lexically_normal());

        if (RelativePath.empty() || RelativePath.is_absolute())
        {
            return false;
        }

        for (const auto& Part : RelativePath)
        {
            if (Part == "..")
            {
                return false;
            }
        }

        const std::filesystem::path DestinationPath = DestinationRoot / RelativePath;
        std::filesystem::create_directories(DestinationPath.parent_path(), FileSystemError);

        if (FileSystemError)
        {
            return false;
        }

        if (std::filesystem::exists(DestinationPath, FileSystemError))
        {
            return !FileSystemError;
        }

        std::filesystem::copy_file(SourceFile, DestinationPath, std::filesystem::copy_options::none, FileSystemError);
        return !FileSystemError;
    }

    inline UStaticMesh* ImportExternalOBJ(const std::filesystem::path& SourcePath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (SourcePath.empty() || !HasOBJExtension(SourcePath))
        {
            return nullptr;
        }

        std::error_code FileSystemError;
        const std::filesystem::path WorkingDirectory = std::filesystem::current_path(FileSystemError);

        if (FileSystemError)
        {
            UE_LOG(Error, "[OBJ Import] Failed to get working directory: %s", FileSystemError.message().c_str());
            return nullptr;
        }

        UE_LOG(Info, "[OBJ Import] WorkingDirectory: %s", PathToUTF8(WorkingDirectory).c_str());
        UE_LOG(Info, "[OBJ Import] SourcePath: %s", PathToUTF8(SourcePath).c_str());

        FileSystemError.clear();

        if (!std::filesystem::is_regular_file(SourcePath, FileSystemError) || FileSystemError)
        {
            UE_LOG(Error, "[OBJ Import] Source OBJ does not exist: %s", PathToUTF8(SourcePath).c_str());
            return nullptr;
        }

        FObjRawData RawData;

        if (!ParseRawOBJ(SourcePath, RawData))
        {
            UE_LOG(Error, "[OBJ Import] ParseRawOBJ failed: %s", PathToUTF8(SourcePath).c_str());
            return nullptr;
        }

        UE_LOG(Info, "[OBJ Import] Parsed: Positions=%zu, Faces=%zu, Materials=%zu", RawData.Positions.size(), RawData.Faces.size(), RawData.Materials.size());

        const std::filesystem::path AbsoluteSourcePath = std::filesystem::absolute(SourcePath, FileSystemError);

        if (FileSystemError)
        {
            return nullptr;
        }

        const std::filesystem::path SourceRoot = AbsoluteSourcePath.parent_path();
        const std::filesystem::path ImportDirectory = std::filesystem::current_path() / "Content" / "Imported";

        std::filesystem::create_directories(ImportDirectory, FileSystemError);

        if (FileSystemError)
        {
            return nullptr;
        }

        const std::string BaseName = SanitizeFileName(PathToUTF8(SourcePath.stem()));
        std::filesystem::path DestinationRoot = ImportDirectory / BaseName;

        for (uint32 Suffix = 1; std::filesystem::exists(DestinationRoot, FileSystemError); ++Suffix)
        {
            DestinationRoot = ImportDirectory / (BaseName + "_" + std::to_string(Suffix));

            if (FileSystemError)
            {
                return nullptr;
            }
        }

        std::filesystem::create_directories(DestinationRoot, FileSystemError);

        if (FileSystemError)
        {
            return nullptr;
        }

        const std::filesystem::path DestinationOBJPath = DestinationRoot / AbsoluteSourcePath.filename();

        std::filesystem::copy_file(AbsoluteSourcePath, DestinationOBJPath, std::filesystem::copy_options::none, FileSystemError);

        if (FileSystemError)
        {
            std::filesystem::remove_all(DestinationRoot, FileSystemError);
            return nullptr;
        }

        for (const std::filesystem::path& DependencyPath : RawData.DependencyFiles)
        {
            if (!CopyDependencyFile(DependencyPath, SourceRoot, DestinationRoot))
            {
                std::filesystem::remove_all(DestinationRoot, FileSystemError);
                return nullptr;
            }
        }

        FString AssetPath;

        if (!GetContentAssetPath(DestinationOBJPath, AssetPath))
        {
            std::filesystem::remove_all(DestinationRoot, FileSystemError);
            return nullptr;
        }

        UStaticMesh* ImportedMesh = LoadOBJAsset(AssetPath, AssetManager, Renderer);

        if (ImportedMesh == nullptr)
        {
            UE_LOG(Error, "[OBJ Import] LoadOBJAsset failed: %s", AssetPath.ToStd().c_str());
            std::filesystem::remove_all(DestinationRoot, FileSystemError);
            return nullptr;
        }

        UE_LOG(Info, "[OBJ Import] Success: %s", AssetPath.ToStd().c_str());

        AssetManager.RefreshAssetRegistry();
        return ImportedMesh;
    }
}