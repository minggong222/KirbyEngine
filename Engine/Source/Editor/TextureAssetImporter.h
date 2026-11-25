#pragma once

#include "Resources/AssetManager.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"
#include "Renderer/Renderer.h"
#include "Core/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <system_error>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace TextureImporter
{
    inline constexpr std::uintmax_t MaxTextureFileBytes = 128ull * 1024ull * 1024ull;

    inline std::string PathToUTF8(const std::filesystem::path& Path)
    {
        const std::u8string UTF8Path = Path.generic_u8string();
        return std::string(reinterpret_cast<const char*>(UTF8Path.data()), UTF8Path.size());
    }

    inline bool HasSupportedTextureExtension(const std::filesystem::path& FilePath)
    {
        std::string Extension = PathToUTF8(FilePath.extension());

        std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });

        return Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" || Extension == ".bmp" || Extension == ".tif" || Extension == ".tiff" || Extension == ".gif";
    }

    inline bool IsValidContentAssetPath(const FString& AssetPath, std::filesystem::path& OutRelativePath)
    {
        if (AssetPath.IsEmpty())
        {
            return false;
        }

        OutRelativePath = FPathUtils::FromUTF8(AssetPath.ToStd());

        if (OutRelativePath.empty() || OutRelativePath.is_absolute())
        {
            return false;
        }

        OutRelativePath = OutRelativePath.lexically_normal();

        for (const auto& Part : OutRelativePath)
        {
            if (Part == "..")
            {
                return false;
            }
        }

        return PathToUTF8(OutRelativePath).starts_with("Content/");
    }

    inline FString GetMaterialAssetPath(const FString& TextureAssetPath)
    {
        return FString(std::string("Content/Materials/") + TextureAssetPath.ToStd() + ".kmat");
    }

    inline UTexture2D* LoadTextureAsset(const FString& AssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        std::filesystem::path RelativePath;

        if (!IsValidContentAssetPath(AssetPath, RelativePath) || !HasSupportedTextureExtension(RelativePath))
        {
            return nullptr;
        }

        const std::filesystem::path FullPath = std::filesystem::current_path() / RelativePath;
        std::error_code FileSystemError;

        if (!std::filesystem::is_regular_file(FullPath, FileSystemError) || FileSystemError)
        {
            return nullptr;
        }

        const std::uintmax_t FileSize = std::filesystem::file_size(FullPath, FileSystemError);

        if (FileSystemError || FileSize == 0 || FileSize > MaxTextureFileBytes)
        {
            return nullptr;
        }

        return AssetManager.Load<UTexture2D>(AssetPath, [&]() -> UTexture2D*
        {
            UTexture2D* NewTexture = NewObject<UTexture2D>();

            if (NewTexture == nullptr)
            {
                return nullptr;
            }

            if (!NewTexture->Initialize(Renderer, FullPath))
            {
                delete NewTexture;
                return nullptr;
            }

            return NewTexture;
        });
    }

    inline bool ReloadTextureAsset(UTexture2D* Texture, URenderer& Renderer)
    {
        if (Texture == nullptr)
        {
            return false;
        }

        std::filesystem::path RelativePath;
        if (!IsValidContentAssetPath(Texture->GetAssetPath(), RelativePath) || !HasSupportedTextureExtension(RelativePath)) return false;

        const std::filesystem::path FullPath = std::filesystem::current_path() / RelativePath;
        std::error_code FileSystemError;
        if (!std::filesystem::is_regular_file(FullPath, FileSystemError) || FileSystemError) return false;

        const std::uintmax_t FileSize = std::filesystem::file_size(FullPath, FileSystemError);
        if (FileSystemError || FileSize == 0 || FileSize > MaxTextureFileBytes) return false;
        return Texture->Reload(Renderer, FullPath);
    }

    inline bool SaveMaterialAsset(UMaterial* Material)
    {
        if (Material == nullptr || Material->GetAssetPath().IsEmpty())
        {
            return false;
        }

        const std::string MaterialAssetPath = Material->GetAssetPath().ToStd();
        std::filesystem::path RelativePath = FPathUtils::FromUTF8(MaterialAssetPath).lexically_normal();

        if (RelativePath.is_absolute() || !PathToUTF8(RelativePath).starts_with("Content/Materials/") || PathToUTF8(RelativePath.extension()) != ".kmat")
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

        const std::filesystem::path FilePath = std::filesystem::current_path() / RelativePath;
        std::error_code FileSystemError;
        std::filesystem::create_directories(FilePath.parent_path(), FileSystemError);

        if (FileSystemError)
        {
            return false;
        }

        const UTexture2D* Texture = Material->GetBaseColorTexture();
        const std::string TextureAssetPath = Texture != nullptr ? Texture->GetAssetPath().ToStd() : std::string();
        std::filesystem::path TemporaryPath = FilePath;
        TemporaryPath += ".tmp";

        {
            std::ofstream OutputFile(TemporaryPath, std::ios::binary | std::ios::trunc);

            if (!OutputFile.is_open())
            {
                return false;
            }

            OutputFile << "KIRBY_MATERIAL 1\n";
            OutputFile << std::quoted(TextureAssetPath) << '\n';
            OutputFile.flush();

            if (!OutputFile.good())
            {
                OutputFile.close();
                std::filesystem::remove(TemporaryPath, FileSystemError);
                return false;
            }
        }

        if (!MoveFileExW(TemporaryPath.c_str(), FilePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            std::filesystem::remove(TemporaryPath, FileSystemError);
            return false;
        }

        return true;
    }

    inline UMaterial* LoadMaterialAsset(const FString& MaterialAssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        std::filesystem::path RelativePath;

        if (!IsValidContentAssetPath(MaterialAssetPath, RelativePath) || PathToUTF8(RelativePath.extension()) != ".kmat" || !PathToUTF8(RelativePath).starts_with("Content/Materials/"))
        {
            return nullptr;
        }

        const std::filesystem::path FullPath = std::filesystem::current_path() / RelativePath;
        std::error_code FileSystemError;

        if (!std::filesystem::is_regular_file(FullPath, FileSystemError) || FileSystemError)
        {
            return nullptr;
        }

        return AssetManager.Load<UMaterial>(MaterialAssetPath, [&]() -> UMaterial*
        {
            std::ifstream InputFile(FullPath, std::ios::binary);
            std::string Magic;
            int32 Version = 0;
            std::string SavedTexturePath;

            if (!InputFile.is_open() || !(InputFile >> Magic >> Version) || Magic != "KIRBY_MATERIAL" || Version != 1 || !(InputFile >> std::quoted(SavedTexturePath)))
            {
                return nullptr;
            }

            UTexture2D* Texture = nullptr;

            if (!SavedTexturePath.empty())
            {
                Texture = LoadTextureAsset(FString(SavedTexturePath), AssetManager, Renderer);

                if (Texture == nullptr)
                {
                    return nullptr;
                }
            }

            UMaterial* NewMaterial = NewObject<UMaterial>();

            if (NewMaterial == nullptr)
            {
                return nullptr;
            }

            NewMaterial->SetBaseColorTexture(Texture);
            return NewMaterial;
        });
    }

    inline bool ReloadMaterialAsset(UMaterial* Material, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (Material == nullptr || Material->GetAssetPath().IsEmpty()) return false;

        std::filesystem::path RelativePath;
        if (!IsValidContentAssetPath(Material->GetAssetPath(), RelativePath) || PathToUTF8(RelativePath.extension()) != ".kmat" || !PathToUTF8(RelativePath).starts_with("Content/Materials/")) return false;

        std::ifstream InputFile(std::filesystem::current_path() / RelativePath, std::ios::binary);
        std::string Magic;
        int32 Version = 0;
        std::string SavedTexturePath;
        if (!InputFile.is_open() || !(InputFile >> Magic >> Version) || Magic != "KIRBY_MATERIAL" || Version != 1 || !(InputFile >> std::quoted(SavedTexturePath))) return false;

        UTexture2D* Texture = nullptr;
        if (!SavedTexturePath.empty())
        {
            Texture = LoadTextureAsset(FString(SavedTexturePath), AssetManager, Renderer);
            if (Texture == nullptr) return false;
        }

        Material->SetBaseColorTexture(Texture);
        return true;
    }

    inline UMaterial* LoadTextureMaterial(const FString& TextureAssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        UTexture2D* Texture = LoadTextureAsset(TextureAssetPath, AssetManager, Renderer);

        if (Texture == nullptr)
        {
            return nullptr;
        }

        const FString MaterialAssetPath = GetMaterialAssetPath(TextureAssetPath);
        std::filesystem::path MaterialRelativePath;
        const bool bValidMaterialPath = IsValidContentAssetPath(MaterialAssetPath, MaterialRelativePath);
        const std::filesystem::path MaterialFilePath = bValidMaterialPath ? std::filesystem::current_path() / MaterialRelativePath : std::filesystem::path();
        std::error_code FileSystemError;

        if (bValidMaterialPath && std::filesystem::is_regular_file(MaterialFilePath, FileSystemError) && !FileSystemError)
        {
            return LoadMaterialAsset(MaterialAssetPath, AssetManager, Renderer);
        }

        UMaterial* Material = AssetManager.Load<UMaterial>(MaterialAssetPath, [Texture]() -> UMaterial*
        {
            UMaterial* NewMaterial = NewObject<UMaterial>();

            if (NewMaterial != nullptr)
            {
                NewMaterial->SetBaseColorTexture(Texture);
            }

            return NewMaterial;
        });

        if (Material != nullptr)
        {
            SaveMaterialAsset(Material);
        }

        return Material;
    }

    inline void RegisterImportedTextureMaterials(FAssetManager& AssetManager, URenderer& Renderer)
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

            if (!It->is_regular_file(EntryError) || EntryError || !HasSupportedTextureExtension(It->path()))
            {
                continue;
            }

            const std::string AssetPath = PathToUTF8(It->path().lexically_relative(std::filesystem::current_path()));
            LoadTextureMaterial(FString(AssetPath), AssetManager, Renderer);
        }

        AssetManager.RefreshAssetRegistry();
        TArray<FAssetRegistryEntry> Entries;
        AssetManager.GetAssetRegistryEntries(Entries);

        for (const FAssetRegistryEntry& Entry : Entries)
        {
            if (Entry.AssetType == "Material")
            {
                LoadMaterialAsset(FString(Entry.AssetPath), AssetManager, Renderer);
            }
        }

        AssetManager.RefreshAssetRegistry();
    }

    inline UMaterial* ImportExternalTexture(const std::filesystem::path& SourcePath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (SourcePath.empty() || !HasSupportedTextureExtension(SourcePath))
        {
            return nullptr;
        }

        std::error_code FileSystemError;

        if (!std::filesystem::is_regular_file(SourcePath, FileSystemError) || FileSystemError)
        {
            return nullptr;
        }

        const std::uintmax_t SourceFileSize = std::filesystem::file_size(SourcePath, FileSystemError);

        if (FileSystemError || SourceFileSize == 0 || SourceFileSize > MaxTextureFileBytes)
        {
            return nullptr;
        }

        const std::filesystem::path ImportDirectory = std::filesystem::current_path() / "Content" / "Imported";
        std::filesystem::create_directories(ImportDirectory, FileSystemError);

        if (FileSystemError)
        {
            return nullptr;
        }

        const std::string Stem = PathToUTF8(SourcePath.stem());
        const std::string Extension = PathToUTF8(SourcePath.extension());
        std::filesystem::path DestinationPath = ImportDirectory / SourcePath.filename();

        for (uint32 Suffix = 1; std::filesystem::exists(DestinationPath); ++Suffix)
        {
            DestinationPath = ImportDirectory / FPathUtils::FromUTF8(Stem + "_" + std::to_string(Suffix) + Extension);
        }

        std::filesystem::copy_file(SourcePath, DestinationPath, std::filesystem::copy_options::none, FileSystemError);

        if (FileSystemError)
        {
            return nullptr;
        }

        const std::string AssetPath = PathToUTF8(DestinationPath.lexically_relative(std::filesystem::current_path()));
        UMaterial* ImportedMaterial = LoadTextureMaterial(FString(AssetPath), AssetManager, Renderer);

        if (ImportedMaterial == nullptr)
        {
            std::filesystem::remove(DestinationPath, FileSystemError);
            return nullptr;
        }

        AssetManager.RefreshAssetRegistry();
        return ImportedMaterial;
    }
}
