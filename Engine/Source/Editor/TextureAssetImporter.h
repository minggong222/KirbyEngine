#pragma once

#include "Resources/AssetManager.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"
#include "Renderer/Renderer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
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

    inline FString GetMaterialAssetPath(const FString& TextureAssetPath)
    {
        return FString(std::string("Content/Materials/") + TextureAssetPath.ToStd() + ".kmat");
    }

    inline UTexture2D* LoadTextureAsset(const FString& AssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        if (AssetPath.IsEmpty())
        {
            return nullptr;
        }

        const std::filesystem::path RelativePath = std::filesystem::u8path(AssetPath.ToStd());

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

    inline bool SaveMaterialAsset(UMaterial* Material)
    {
        if (Material == nullptr || Material->GetAssetPath().IsEmpty())
        {
            return false;
        }

        const std::string MaterialAssetPath = Material->GetAssetPath().ToStd();

        if (!MaterialAssetPath.starts_with("Content/Materials/") || !MaterialAssetPath.ends_with(".kmat"))
        {
            return false;
        }

        const std::filesystem::path FilePath = std::filesystem::current_path() / std::filesystem::u8path(MaterialAssetPath);
        const std::filesystem::path ParentPath = FilePath.parent_path();

        std::error_code FileSystemError;
        std::filesystem::create_directories(ParentPath, FileSystemError);

        if (FileSystemError)
        {
            return false;
        }

        const UTexture2D* Texture = Material->GetBaseColorTexture();
        const std::string TextureAssetPath = Texture != nullptr ? Texture->GetAssetPath().ToStd() : std::string();

        std::ofstream OutputFile(FilePath, std::ios::binary | std::ios::trunc);

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
            return false;
        }

        OutputFile.close();

        return !OutputFile.fail();
    }

    inline UMaterial* LoadTextureMaterial(const FString& TextureAssetPath, FAssetManager& AssetManager, URenderer& Renderer)
    {
        UTexture2D* Texture = LoadTextureAsset(TextureAssetPath, AssetManager, Renderer);

        if (Texture == nullptr)
        {
            return nullptr;
        }

        const FString MaterialAssetPath = GetMaterialAssetPath(TextureAssetPath);

        return AssetManager.Load<UMaterial>(MaterialAssetPath, [&]() -> UMaterial*
        {
            UTexture2D* MaterialTexture = Texture;

            const std::filesystem::path MaterialFilePath = std::filesystem::current_path() / std::filesystem::u8path(MaterialAssetPath.ToStd());

            std::error_code FileSystemError;

            if (std::filesystem::is_regular_file(MaterialFilePath, FileSystemError) && !FileSystemError)
            {
                std::ifstream InputFile(MaterialFilePath, std::ios::binary);

                std::string Magic;
                int32 Version = 0;
                std::string SavedTexturePath;

                if (!InputFile.is_open() || !(InputFile >> Magic >> Version) || Magic != "KIRBY_MATERIAL" || Version != 1 || !(InputFile >> std::quoted(SavedTexturePath)))
                {
                    return nullptr;
                }

                if (!SavedTexturePath.empty())
                {
                    MaterialTexture = LoadTextureAsset(FString(SavedTexturePath), AssetManager, Renderer);

                    if (MaterialTexture == nullptr)
                    {
                        return nullptr;
                    }
                }
                else
                {
                    MaterialTexture = nullptr;
                }
            }

            UMaterial* NewMaterial = NewObject<UMaterial>();

            if (NewMaterial == nullptr)
            {
                return nullptr;
            }

            NewMaterial->SetBaseColorTexture(MaterialTexture);

            return NewMaterial;
        });
    }

    inline void RegisterImportedTextureMaterials(FAssetManager& AssetManager, URenderer& Renderer)
    {
        const std::filesystem::path ImportDirectory = std::filesystem::current_path() / "Content" / "Imported";

        std::error_code DirectoryError;

        if (!std::filesystem::is_directory(ImportDirectory, DirectoryError) || DirectoryError)
        {
            return;
        }

        for (std::filesystem::directory_iterator It(ImportDirectory, DirectoryError), End; !DirectoryError && It != End; It.increment(DirectoryError))
        {
            std::error_code EntryError;

            if (!It->is_regular_file(EntryError) || EntryError || !HasSupportedTextureExtension(It->path()))
            {
                continue;
            }

            const FString AssetPath((std::filesystem::path("Content") / "Imported" / It->path().filename()).generic_string());

            LoadTextureMaterial(AssetPath, AssetManager, Renderer);
        }
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
