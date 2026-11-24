#pragma once

#include "Resources/AssetManager.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"
#include "Renderer/Renderer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

namespace TextureImporter
{
    inline constexpr std::uintmax_t MaxTextureFileBytes = 128ull * 1024ull * 1024ull;

    inline bool HasSupportedTextureExtension(const std::filesystem::path& FilePath)
    {
        std::string Extension = FilePath.extension().string();

        std::transform(Extension.begin(), Extension.end(), Extension.begin(), [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });

        return Extension == ".png" || Extension == ".jpg" || Extension == ".jpeg" || Extension == ".bmp" || Extension == ".tif" || Extension == ".tiff" || Extension == ".gif";
    }

    inline UTexture2D* LoadTextureAsset(const FString& AssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (AssetPath.IsEmpty())
        {
            return nullptr;
        }

        const std::filesystem::path RelativePath(AssetPath.ToStd());

        if (RelativePath.empty() || RelativePath.is_absolute() || !HasSupportedTextureExtension(RelativePath))
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

    inline UMaterial* LoadTextureMaterial(const FString& TextureAssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        UTexture2D* Texture = LoadTextureAsset(TextureAssetPath, AssetManager, Renderer);

        if (Texture == nullptr)
        {
            return nullptr;
        }

        const FString MaterialAssetPath(std::string("/Engine/ImportedMaterials/") + Texture->GetAssetPath().ToStd());

        return AssetManager.Load<UMaterial>(MaterialAssetPath, [Texture]() -> UMaterial*
        {
            UMaterial* NewMaterial = NewObject<UMaterial>();

            if (NewMaterial == nullptr)
            {
                return nullptr;
            }

            NewMaterial->SetBaseColorTexture(Texture);

            return NewMaterial;
        });
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

        UMaterial* ImportedMaterial = LoadTextureMaterial(AssetPath, AssetManager, Renderer);

        if (ImportedMaterial == nullptr)
        {
            std::filesystem::remove(DestinationPath, FileSystemError);
            return nullptr;
        }

        return ImportedMaterial;
    }
}