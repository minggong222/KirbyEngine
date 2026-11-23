#pragma once

#include "Renderer/Renderer.h"
#include "Resources/AssetManager.h"
#include "Resources/PrimitiveMeshAssets.h"
#include "Resources/StaticMesh.h"

#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace OBJImporter
{
    inline constexpr std::uintmax_t MaxOBJFileBytes = 64ull * 1024ull * 1024ull;
    inline constexpr std::size_t MaxOBJPositionCount = 1'000'000;
    inline constexpr std::size_t MaxOBJFaceVertexCount = 4096;
    inline constexpr int32 MaxOBJOutputIndexCount = 3'000'000;
    
    inline bool HasOBJExtension(const std::filesystem::path& FilePath)
    {
        std::string Extension = FilePath.extension().string();

        std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });

        return Extension == ".obj";
    }
    
    inline bool ResolveVertexIndex(std::string_view Token, int32 VertexCount, int32& OutIndex)
    {
        const std::size_t SlashPosition = Token.find('/');

        if (SlashPosition != std::string_view::npos)
        {
            Token = Token.substr(0, SlashPosition);
        }

        if (Token.empty())
        {
            return false;
        }

        int32 RawIndex = 0;

        const auto Result = std::from_chars(Token.data(), Token.data() + Token.size(), RawIndex);

        if (Result.ec != std::errc{} || Result.ptr != Token.data() + Token.size() || RawIndex == 0)
        {
            return false;
        }

        OutIndex = RawIndex > 0 ? RawIndex - 1 : VertexCount + RawIndex;

        return OutIndex >= 0 && OutIndex < VertexCount;
    }

    inline bool ParseOBJ(const std::filesystem::path& FilePath, FStaticMeshBuildData& OutMeshData)
    {
        OutMeshData.Vertices.Reset();
        OutMeshData.Indices.Reset();

        std::error_code FileSystemError;
        const std::uintmax_t FileSize = std::filesystem::file_size(FilePath, FileSystemError);

        if (FileSystemError || FileSize == 0 || FileSize > MaxOBJFileBytes)
        {
            return false;
        }

        std::ifstream InputFile(FilePath);

        if (!InputFile.is_open())
        {
            return false;
        }

        std::vector<FVector> Positions;
        TMap<int32, uint32> PositionToVertexIndex;

        FStaticMeshBuildData ParsedData;

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
                if (Positions.size() >= MaxOBJPositionCount)
                {
                    return false;
                }

                float X = 0.0f;
                float Y = 0.0f;
                float Z = 0.0f;

                if (!(LineStream >> X >> Y >> Z))
                {
                    return false;
                }

                if (!std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z))
                {
                    return false;
                }

                Positions.emplace_back(X, Y, Z);
            }
            else if (Command == "f")
            {
                std::vector<uint32> FaceIndices;
                std::string Token;

                while (LineStream >> Token)
                {
                    if (!Token.empty() && Token[0] == '#')
                    {
                        break;
                    }

                    if (FaceIndices.size() >= MaxOBJFaceVertexCount)
                    {
                        return false;
                    }

                    int32 PositionIndex = INDEX_NONE;

                    if (!ResolveVertexIndex(Token, static_cast<int32>(Positions.size()), PositionIndex))
                    {
                        return false;
                    }

                    uint32* ExistingVertexIndex = PositionToVertexIndex.Find(PositionIndex);

                    if (ExistingVertexIndex != nullptr)
                    {
                        FaceIndices.push_back(*ExistingVertexIndex);
                    }
                    else
                    {
                        const FVector& Position = Positions[PositionIndex];

                        const uint32 NewVertexIndex = static_cast<uint32>(ParsedData.Vertices.Add({Position.x, Position.y, Position.z, 1.0f, 1.0f, 1.0f, 1.0f}));

                        PositionToVertexIndex.Add(PositionIndex, NewVertexIndex);
                        FaceIndices.push_back(NewVertexIndex);
                    }
                }

                if (FaceIndices.size() < 3)
                {
                    return false;
                }

                const std::size_t RequiredIndexCount = (FaceIndices.size() - 2) * 3;

                if (RequiredIndexCount > static_cast<std::size_t>(MaxOBJOutputIndexCount - ParsedData.Indices.Num()))
                {
                    return false;
                }

                for (std::size_t Index = 1; Index + 1 < FaceIndices.size(); ++Index)
                {
                    ParsedData.Indices.Add(FaceIndices[0]);
                    ParsedData.Indices.Add(FaceIndices[Index]);
                    ParsedData.Indices.Add(FaceIndices[Index + 1]);
                }
            }
        }

        if (InputFile.bad() || ParsedData.Vertices.Num() < 3 || ParsedData.Indices.Num() < 3 || ParsedData.Indices.Num() % 3 != 0)
        {
            return false;
        }

        OutMeshData = std::move(ParsedData);

        return true;
    }

    inline UStaticMesh* LoadOBJAsset(const FString& AssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (AssetPath == BuiltinAssetPaths::Cube)
        {
            return AssetManager.Find<UStaticMesh>(BuiltinAssetPaths::Cube);
        }

        if (AssetPath == BuiltinAssetPaths::Sphere)
        {
            return AssetManager.Find<UStaticMesh>(BuiltinAssetPaths::Sphere);
        }

        if (AssetPath == BuiltinAssetPaths::Plane)
        {
            return AssetManager.Find<UStaticMesh>(BuiltinAssetPaths::Plane);
        }

        const std::filesystem::path RelativePath(AssetPath.ToStd());

        if (RelativePath.empty() || RelativePath.is_absolute() || !HasOBJExtension(RelativePath))
        {
            return nullptr;
        }

        const std::filesystem::path NormalizedPath = RelativePath.lexically_normal();

        for (const auto& Part : NormalizedPath)
        {
            if (Part == "..")
            {
                return nullptr;
            }
        }

        const std::string NormalizedAssetPath = NormalizedPath.generic_string();

        if (!NormalizedAssetPath.starts_with("Content/Imported/"))
        {
            return nullptr;
        }

        const std::filesystem::path FullPath = std::filesystem::current_path() / NormalizedPath;
        std::error_code FileSystemError;

        if (!std::filesystem::is_regular_file(FullPath, FileSystemError) || FileSystemError)
        {
            return nullptr;
        }

        return AssetManager.Load<UStaticMesh>(AssetPath, [&]() -> UStaticMesh*
        {
            FStaticMeshBuildData MeshData;

            if (!ParseOBJ(FullPath, MeshData))
            {
                return nullptr;
            }

            UStaticMesh* NewMesh = NewObject<UStaticMesh>();

            if (NewMesh == nullptr)
            {
                return nullptr;
            }

            if (!NewMesh->Initialize(Renderer, std::move(MeshData)))
            {
                delete NewMesh;
                return nullptr;
            }

            return NewMesh;
        });
    }

    inline UStaticMesh* ImportExternalOBJ(const std::filesystem::path& SourcePath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (SourcePath.empty() || !HasOBJExtension(SourcePath))
        {
            return nullptr;
        }

        std::error_code FileSystemError;

        if (!std::filesystem::is_regular_file(SourcePath, FileSystemError) || FileSystemError)
        {
            return nullptr;
        }

        const std::uintmax_t SourceFileSize = std::filesystem::file_size(SourcePath, FileSystemError);

        if (FileSystemError || SourceFileSize == 0 || SourceFileSize > MaxOBJFileBytes)
        {
            return nullptr;
        }

        const std::filesystem::path ImportDirectory = std::filesystem::current_path() / "Content" / "Imported";

        std::filesystem::create_directories(ImportDirectory, FileSystemError);

        if (FileSystemError)
        {
            return nullptr;
        }

        const std::string Stem = SourcePath.stem().string();
        const std::string Extension = SourcePath.extension().string();

        std::filesystem::path DestinationPath = ImportDirectory / SourcePath.filename();

        for (uint32 Suffix = 1; std::filesystem::exists(DestinationPath); ++Suffix)
        {
            DestinationPath = ImportDirectory / (Stem + "_" + std::to_string(Suffix) + Extension);
        }

        std::filesystem::copy_file(SourcePath, DestinationPath, std::filesystem::copy_options::none, FileSystemError);

        if (FileSystemError)
        {
            return nullptr;
        }

        const std::filesystem::path RelativePath = std::filesystem::relative(DestinationPath, std::filesystem::current_path(), FileSystemError);

        if (FileSystemError)
        {
            std::filesystem::remove(DestinationPath, FileSystemError);
            return nullptr;
        }

        const FString AssetPath(RelativePath.generic_string());

        UStaticMesh* ImportedMesh = LoadOBJAsset(AssetPath, AssetManager, Renderer);

        if (ImportedMesh == nullptr)
        {
            std::filesystem::remove(DestinationPath, FileSystemError);
            return nullptr;
        }

        return ImportedMesh;
    }
}