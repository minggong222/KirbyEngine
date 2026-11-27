#include "Serialization/Package.h"

#include "Core/PathUtils.h"
#include "Resources/Asset.h"
#include "UObject/Class.h"

FArchive& operator<<(FArchive& Ar, FPackageSummary& Summary)
{
    Ar << Summary.Magic << Summary.Version << Summary.ClassName << Summary.AssetPath << Summary.Dependencies;
    return Ar;
}

namespace PackageFile
{
    std::filesystem::path GetPackagePath(const FString& AssetPath)
    {
        return FPathUtils::FromUTF8(AssetPath.ToStd() + Extension);
    }

    bool Save(UAsset& Asset)
    {
        if (!Asset.SupportsPackage() || Asset.GetAssetPath().IsEmpty() || Asset.GetClass() == nullptr)
        {
            return false;
        }

        FPackageSummary Summary;
        Summary.Magic = Magic;
        Summary.Version = Version;
        Summary.ClassName = Asset.GetClass()->GetName();
        Summary.AssetPath = Asset.GetAssetPath();

        TArray<UAsset*> Dependencies;
        Asset.GetDependencies(Dependencies);

        for (const UAsset* Dependency : Dependencies)
        {
            if (Dependency != nullptr && !Dependency->GetAssetPath().IsEmpty())
            {
                Summary.Dependencies.Add(Dependency->GetAssetPath());
            }
        }

        const std::filesystem::path PackagePath = GetPackagePath(Asset.GetAssetPath());
        const std::filesystem::path TempPath = PackagePath.string() + ".tmp";

        {
            FArchiveFileWriter Writer(TempPath);
            Writer << Summary;
            Asset.Serialize(Writer);

            if (Writer.IsError())
            {
                std::error_code Ignored;
                std::filesystem::remove(TempPath, Ignored);
                return false;
            }
        }

        // Replace the old package only once the new one is complete.
        std::error_code Error;
        std::filesystem::rename(TempPath, PackagePath, Error);

        if (Error)
        {
            std::filesystem::remove(TempPath, Error);
            return false;
        }

        return true;
    }

    bool ReadSummary(const std::filesystem::path& PackagePath, FPackageSummary& OutSummary)
    {
        FArchiveFileReader Reader(PackagePath);
        Reader << OutSummary;
        return !Reader.IsError() && OutSummary.Magic == Magic && OutSummary.Version == Version;
    }
}
