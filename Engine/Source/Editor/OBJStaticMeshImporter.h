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

namespace OBJImporter
{
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

    inline bool ParseOBJ(const std::filesystem::path& FilePath, TArray<FVertexSimple>& OutVertices)
    {
        std::ifstream InputFile(FilePath);

        if (!InputFile.is_open())
        {
            return false;
        }

        std::vector<FVector> Positions;

        std::string Line;

        while (std::getline(InputFile, Line))
        {
            if (Line.empty() || Line[0] == '#')
            {
                continue;
            }

            std::istringstream LineStream(Line);
            std::string Command;

            LineStream >> Command;

            if (Command == "v")
            {
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
                std::vector<int32> FaceIndices;
                std::string Token;

                while (LineStream >> Token)
                {
                    if (!Token.empty() && Token[0] == '#')
                    {
                        break;
                    }

                    int32 VertexIndex = INDEX_NONE;

                    if (!ResolveVertexIndex(Token, static_cast<int32>(Positions.size()), VertexIndex))
                    {
                        return false;
                    }

                    FaceIndices.push_back(VertexIndex);
                }

                if (FaceIndices.size() < 3)
                {
                    return false;
                }

                for (std::size_t Index = 1; Index + 1 < FaceIndices.size(); ++Index)
                {
                    const FVector& A = Positions[FaceIndices[0]];
                    const FVector& B = Positions[FaceIndices[Index]];
                    const FVector& C = Positions[FaceIndices[Index + 1]];

                    OutVertices.Add({A.x, A.y, A.z, 1.0f, 1.0f, 1.0f, 1.0f});
                    OutVertices.Add({B.x, B.y, B.z, 1.0f, 1.0f, 1.0f, 1.0f});
                    OutVertices.Add({C.x, C.y, C.z, 1.0f, 1.0f, 1.0f, 1.0f});
                }
            }
        }

        return !InputFile.bad() && OutVertices.Num() >= 3 && OutVertices.Num() % 3 == 0;
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

        if (RelativePath.is_absolute())
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

        const std::filesystem::path FullPath = std::filesystem::current_path() / NormalizedPath;

        return AssetManager.Load<UStaticMesh>(AssetPath, [&]() -> UStaticMesh*
        {
            TArray<FVertexSimple> Vertices;

            if (!ParseOBJ(FullPath, Vertices))
            {
                return nullptr;
            }

            UStaticMesh* NewMesh = NewObject<UStaticMesh>();

            if (NewMesh == nullptr)
            {
                return nullptr;
            }

            if (!NewMesh->Initialize(Renderer, std::move(Vertices)))
            {
                delete NewMesh;
                return nullptr;
            }

            return NewMesh;
        });
    }

    inline UStaticMesh* ImportExternalOBJ(const std::filesystem::path& SourcePath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (SourcePath.empty() || !std::filesystem::exists(SourcePath) || SourcePath.extension() != ".obj")
        {
            return nullptr;
        }

        std::error_code FileSystemError;

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