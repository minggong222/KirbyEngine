#pragma once

#include "Renderer/Renderer.h"
#include "Core/PathUtils.h"
#include "Resources/AssetManager.h"
#include "Resources/PrimitiveMeshAssets.h"
#include "Resources/StaticMesh.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace OBJImporter
{
    inline constexpr std::uintmax_t MaxOBJFileBytes = 64ull * 1024ull * 1024ull;
    inline constexpr std::size_t MaxOBJPositionCount = 1'000'000;
    inline constexpr std::size_t MaxOBJFaceVertexCount = 4096;
    inline constexpr int32 MaxOBJOutputIndexCount = 3'000'000;

    inline std::string PathToUTF8(const std::filesystem::path& Path)
    {
        const std::u8string UTF8Path = Path.generic_u8string();
        return std::string(reinterpret_cast<const char*>(UTF8Path.data()), UTF8Path.size());
    }

    inline bool HasOBJExtension(const std::filesystem::path& FilePath)
    {
        std::string Extension = PathToUTF8(FilePath.extension());
        std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
        return Extension == ".obj";
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
    };

    inline bool ResolveOBJIndex(std::string_view Token, int32 ElementCount, int32& OutIndex)
    {
        if (Token.empty()) return false;

        int32 RawIndex = 0;
        const auto Result = std::from_chars(Token.data(), Token.data() + Token.size(), RawIndex);

        if (Result.ec != std::errc{} || Result.ptr != Token.data() + Token.size() || RawIndex == 0) return false;

        OutIndex = RawIndex > 0 ? RawIndex - 1 : ElementCount + RawIndex;
        return OutIndex >= 0 && OutIndex < ElementCount;
    }

    inline bool ResolveOBJFaceVertex(std::string_view Token, int32 PositionCount, int32 TexCoordCount, FOBJFaceVertex& OutVertex)
    {
        const std::size_t FirstSlash = Token.find('/');
        const std::string_view PositionToken = FirstSlash == std::string_view::npos ? Token : Token.substr(0, FirstSlash);

        if (!ResolveOBJIndex(PositionToken, PositionCount, OutVertex.PositionIndex)) return false;
        OutVertex.TexCoordIndex = INDEX_NONE;

        if (FirstSlash == std::string_view::npos) return true;

        const std::size_t SecondSlash = Token.find('/', FirstSlash + 1);
        const std::size_t TexCoordStart = FirstSlash + 1;
        const std::size_t TexCoordLength = SecondSlash == std::string_view::npos ? Token.size() - TexCoordStart : SecondSlash - TexCoordStart;
        const std::string_view TexCoordToken = Token.substr(TexCoordStart, TexCoordLength);

        if (TexCoordToken.empty()) return true;
        return ResolveOBJIndex(TexCoordToken, TexCoordCount, OutVertex.TexCoordIndex);
    }

    inline bool ParseOBJ(const std::filesystem::path& FilePath, FStaticMeshBuildData& OutMeshData)
    {
        OutMeshData.Vertices.Reset();
        OutMeshData.Indices.Reset();

        std::error_code FileSystemError;
        const std::uintmax_t FileSize = std::filesystem::file_size(FilePath, FileSystemError);

        if (FileSystemError || FileSize == 0 || FileSize > MaxOBJFileBytes) return false;

        std::ifstream InputFile(FilePath);
        if (!InputFile.is_open()) return false;

        std::vector<FVector> Positions;
        std::vector<FOBJTexCoord> TexCoords;
        TMap<uint64, uint32> VertexMap;
        FStaticMeshBuildData ParsedData;
        std::string Line;

        while (std::getline(InputFile, Line))
        {
            std::istringstream LineStream(Line);
            std::string Command;

            if (!(LineStream >> Command) || Command[0] == '#') continue;

            if (Command == "v")
            {
                if (Positions.size() >= MaxOBJPositionCount) return false;

                float X = 0.0f;
                float Y = 0.0f;
                float Z = 0.0f;

                if (!(LineStream >> X >> Y >> Z) || !std::isfinite(X) || !std::isfinite(Y) || !std::isfinite(Z)) return false;
                Positions.emplace_back(X, Y, Z);
            }
            else if (Command == "vt")
            {
                if (TexCoords.size() >= MaxOBJPositionCount) return false;

                FOBJTexCoord TexCoord;
                if (!(LineStream >> TexCoord.U)) return false;
                if (!(LineStream >> TexCoord.V)) TexCoord.V = 0.0f;
                if (!std::isfinite(TexCoord.U) || !std::isfinite(TexCoord.V)) return false;
                TexCoords.push_back(TexCoord);
            }
            else if (Command == "f")
            {
                std::vector<uint32> FaceIndices;
                std::string Token;

                while (LineStream >> Token)
                {
                    if (!Token.empty() && Token[0] == '#') break;
                    if (FaceIndices.size() >= MaxOBJFaceVertexCount) return false;

                    FOBJFaceVertex FaceVertex;
                    if (!ResolveOBJFaceVertex(Token, static_cast<int32>(Positions.size()), static_cast<int32>(TexCoords.size()), FaceVertex)) return false;

                    const uint32 TexCoordKey = FaceVertex.TexCoordIndex == INDEX_NONE ? 0u : static_cast<uint32>(FaceVertex.TexCoordIndex) + 1u;
                    const uint64 VertexKey = (static_cast<uint64>(static_cast<uint32>(FaceVertex.PositionIndex)) << 32) | TexCoordKey;
                    uint32* ExistingVertexIndex = VertexMap.Find(VertexKey);

                    if (ExistingVertexIndex != nullptr)
                    {
                        FaceIndices.push_back(*ExistingVertexIndex);
                    }
                    else
                    {
                        if (ParsedData.Vertices.Num() >= MaxOBJOutputIndexCount) return false;
                        const FVector& Position = Positions[FaceVertex.PositionIndex];
                        const FOBJTexCoord TexCoord = FaceVertex.TexCoordIndex != INDEX_NONE ? TexCoords[FaceVertex.TexCoordIndex] : FOBJTexCoord{};
                        const uint32 NewVertexIndex = static_cast<uint32>(ParsedData.Vertices.Add({ Position.x, Position.y, Position.z, 1.0f, 1.0f, 1.0f, 1.0f, TexCoord.U, 1.0f - TexCoord.V }));
                        VertexMap.Add(VertexKey, NewVertexIndex);
                        FaceIndices.push_back(NewVertexIndex);
                    }
                }

                if (FaceIndices.size() < 3) return false;
                const std::size_t RequiredIndexCount = (FaceIndices.size() - 2) * 3;
                if (RequiredIndexCount > static_cast<std::size_t>(MaxOBJOutputIndexCount - ParsedData.Indices.Num())) return false;

                for (std::size_t Index = 1; Index + 1 < FaceIndices.size(); ++Index)
                {
                    ParsedData.Indices.Add(FaceIndices[0]);
                    ParsedData.Indices.Add(FaceIndices[Index]);
                    ParsedData.Indices.Add(FaceIndices[Index + 1]);
                }
            }
        }

        if (InputFile.bad() || ParsedData.Vertices.Num() < 3 || ParsedData.Indices.Num() < 3 || ParsedData.Indices.Num() % 3 != 0) return false;
        OutMeshData = std::move(ParsedData);
        return true;
    }

    inline UStaticMesh* LoadOBJAsset(const FString& AssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (UStaticMesh* ExistingMesh = AssetManager.Find<UStaticMesh>(AssetPath)) return ExistingMesh;

        const std::filesystem::path RelativePath = FPathUtils::FromUTF8(AssetPath.ToStd());

        if (RelativePath.empty() || RelativePath.is_absolute() || !HasOBJExtension(RelativePath)) return nullptr;

        const std::filesystem::path NormalizedPath = RelativePath.lexically_normal();
        for (const auto& Part : NormalizedPath) if (Part == "..") return nullptr;
        if (!PathToUTF8(NormalizedPath).starts_with("Content/")) return nullptr;

        const std::filesystem::path FullPath = std::filesystem::current_path() / NormalizedPath;
        std::error_code FileSystemError;
        if (!std::filesystem::is_regular_file(FullPath, FileSystemError) || FileSystemError) return nullptr;

        return AssetManager.Load<UStaticMesh>(AssetPath, [&]() -> UStaticMesh*
        {
            FStaticMeshBuildData MeshData;
            if (!ParseOBJ(FullPath, MeshData)) return nullptr;
            UStaticMesh* NewMesh = NewObject<UStaticMesh>();
            if (NewMesh == nullptr) return nullptr;
            if (!NewMesh->Initialize(Renderer, std::move(MeshData)))
            {
                delete NewMesh;
                return nullptr;
            }
            return NewMesh;
        });
    }

    inline bool ReimportOBJAsset(UStaticMesh* Mesh, URenderer& Renderer)
    {
        if (Mesh == nullptr)
        {
            return false;
        }

        const std::filesystem::path RelativePath = FPathUtils::FromUTF8(Mesh->GetAssetPath().ToStd());

        if (RelativePath.empty() || RelativePath.is_absolute() || !HasOBJExtension(RelativePath))
        {
            return false;
        }

        const std::filesystem::path NormalizedPath = RelativePath.lexically_normal();
        for (const auto& Part : NormalizedPath) if (Part == "..") return false;
        if (!PathToUTF8(NormalizedPath).starts_with("Content/")) return false;

        const std::filesystem::path FullPath = std::filesystem::current_path() / NormalizedPath;
        std::error_code FileSystemError;
        if (!std::filesystem::is_regular_file(FullPath, FileSystemError) || FileSystemError) return false;

        FStaticMeshBuildData MeshData;
        return ParseOBJ(FullPath, MeshData) && Mesh->Reinitialize(Renderer, std::move(MeshData));
    }

    inline void RegisterImportedStaticMeshes(FAssetManager& AssetManager, URenderer& Renderer)
    {
        const std::filesystem::path ContentDirectory = std::filesystem::current_path() / "Content";
        std::error_code DirectoryError;

        if (!std::filesystem::is_directory(ContentDirectory, DirectoryError) || DirectoryError) return;

        for (std::filesystem::recursive_directory_iterator It(ContentDirectory, std::filesystem::directory_options::skip_permission_denied, DirectoryError), End; !DirectoryError && It != End; It.increment(DirectoryError))
        {
            std::error_code EntryError;
            if (!It->is_regular_file(EntryError) || EntryError || !HasOBJExtension(It->path())) continue;
            const FString AssetPath(PathToUTF8(It->path().lexically_relative(std::filesystem::current_path())));
            LoadOBJAsset(AssetPath, AssetManager, Renderer);
        }
    }

    inline UStaticMesh* ImportExternalOBJ(const std::filesystem::path& SourcePath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (SourcePath.empty() || !HasOBJExtension(SourcePath)) return nullptr;

        std::error_code FileSystemError;
        if (!std::filesystem::is_regular_file(SourcePath, FileSystemError) || FileSystemError) return nullptr;
        const std::uintmax_t SourceFileSize = std::filesystem::file_size(SourcePath, FileSystemError);
        if (FileSystemError || SourceFileSize == 0 || SourceFileSize > MaxOBJFileBytes) return nullptr;

        const std::filesystem::path ImportDirectory = std::filesystem::current_path() / "Content" / "Imported";
        std::filesystem::create_directories(ImportDirectory, FileSystemError);
        if (FileSystemError) return nullptr;

        const std::string Stem = PathToUTF8(SourcePath.stem());
        const std::string Extension = PathToUTF8(SourcePath.extension());
        std::filesystem::path DestinationPath = ImportDirectory / SourcePath.filename();

        for (uint32 Suffix = 1; std::filesystem::exists(DestinationPath); ++Suffix)
        {
            DestinationPath = ImportDirectory / FPathUtils::FromUTF8(Stem + "_" + std::to_string(Suffix) + Extension);
        }

        std::filesystem::copy_file(SourcePath, DestinationPath, std::filesystem::copy_options::none, FileSystemError);
        if (FileSystemError) return nullptr;

        const FString AssetPath(PathToUTF8(DestinationPath.lexically_relative(std::filesystem::current_path())));
        UStaticMesh* ImportedMesh = LoadOBJAsset(AssetPath, AssetManager, Renderer);

        if (ImportedMesh == nullptr)
        {
            std::filesystem::remove(DestinationPath, FileSystemError);
            return nullptr;
        }

        AssetManager.RefreshAssetRegistry();
        return ImportedMesh;
    }
}
