#include "AssetManager.h"
#include "Core/PathUtils.h"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <unordered_set>
#include <vector>

FAssetManager::FAssetManager()
{
    RefreshAssetRegistry();
}

std::string FAssetManager::PathToUTF8(const std::filesystem::path& Path)
{
    const std::u8string UTF8Path = Path.generic_u8string();
    return std::string(reinterpret_cast<const char*>(UTF8Path.data()), UTF8Path.size());
}

bool FAssetManager::IsValidContentPath(const std::string& AssetPath) const
{
    if (AssetPath.empty())
    {
        return false;
    }

    const std::filesystem::path Path = FPathUtils::FromUTF8(AssetPath);

    if (Path.is_absolute())
    {
        return false;
    }

    const std::filesystem::path NormalizedPath = Path.lexically_normal();

    for (const auto& Part : NormalizedPath)
    {
        if (Part == "..")
        {
            return false;
        }
    }

    const std::string NormalizedString = PathToUTF8(NormalizedPath);
    return NormalizedString.starts_with("Content/");
}

std::string FAssetManager::GetAssetTypeFromExtension(const std::string& Extension)
{
    std::string LowerExtension = Extension;

    std::transform(LowerExtension.begin(), LowerExtension.end(), LowerExtension.begin(), [](unsigned char Character)
    {
        return static_cast<char>(std::tolower(Character));
    });

    if (LowerExtension == ".obj") return "Static Mesh";
    if (LowerExtension == ".kmat") return "Material";

    if (LowerExtension == ".png" || LowerExtension == ".jpg" || LowerExtension == ".jpeg" || LowerExtension == ".bmp" || LowerExtension == ".tif" || LowerExtension == ".tiff" || LowerExtension == ".gif")
    {
        return "Texture";
    }

    return {};
}

UAsset* FAssetManager::FindAny(const FString& AssetPath) const
{
    UAsset* const* FoundAsset = LoadedAssets.Find(AssetPath.ToStd());
    return FoundAsset != nullptr ? *FoundAsset : nullptr;
}

bool FAssetManager::RepathAsset(UAsset* Asset, const std::string& NewAssetPath)
{
    if (Asset == nullptr || !IsValidContentPath(NewAssetPath) || FindAny(Asset->GetAssetPath()) != Asset)
    {
        return false;
    }

    return RepathAssets(Asset->GetAssetPath().ToStd(), NewAssetPath);
}

bool FAssetManager::RepathAssets(const std::string& OldPrefix, const std::string& NewPrefix)
{
    if (!IsValidContentPath(OldPrefix) || !IsValidContentPath(NewPrefix) || OldPrefix == NewPrefix)
    {
        return false;
    }

    if (NewPrefix.starts_with(OldPrefix + "/"))
    {
        return false;
    }

    struct FRepathEntry
    {
        std::string OldPath;
        std::string NewPath;
        UAsset* Asset = nullptr;
    };

    std::vector<FRepathEntry> Entries;
    std::unordered_set<std::string> NewPaths;

    for (const auto& Pair : LoadedAssets)
    {
        if (Pair.first == OldPrefix || Pair.first.starts_with(OldPrefix + "/"))
        {
            FRepathEntry Entry;
            Entry.OldPath = Pair.first;
            Entry.NewPath = NewPrefix + Pair.first.substr(OldPrefix.size());
            Entry.Asset = Pair.second;

            if (!IsValidContentPath(Entry.NewPath) || !NewPaths.insert(Entry.NewPath).second)
            {
                return false;
            }

            Entries.push_back(std::move(Entry));
        }
    }

    for (const FRepathEntry& Entry : Entries)
    {
        if (!LoadedAssets.Contains(Entry.NewPath))
        {
            continue;
        }

        const bool bDestinationIsAlsoMoving = std::any_of(Entries.begin(), Entries.end(), [&Entry](const FRepathEntry& Candidate)
        {
            return Candidate.OldPath == Entry.NewPath;
        });

        if (!bDestinationIsAlsoMoving)
        {
            return false;
        }
    }

    for (const FRepathEntry& Entry : Entries)
    {
        LoadedAssets.Remove(Entry.OldPath);
    }

    for (const FRepathEntry& Entry : Entries)
    {
        Entry.Asset->AssetPath = FString(Entry.NewPath);
        LoadedAssets.Add(Entry.NewPath, Entry.Asset);
    }

    RefreshAssetRegistry();
    return true;
}

bool FAssetManager::RemoveAsset(UAsset* Asset)
{
    if (Asset == nullptr)
    {
        return false;
    }

    TArray<UAsset*> Referencers;
    GetReferencers(Asset, Referencers);

    if (!Referencers.IsEmpty())
    {
        return false;
    }

    const std::string AssetPath = Asset->GetAssetPath().ToStd();

    if (LoadedAssets.Remove(AssetPath) == 0)
    {
        return false;
    }

    delete Asset;
    RefreshAssetRegistry();
    return true;
}

void FAssetManager::GetDependencies(UAsset* Asset, TArray<UAsset*>& OutDependencies) const
{
    OutDependencies.Reset();

    if (Asset != nullptr)
    {
        Asset->GetDependencies(OutDependencies);
    }
}

void FAssetManager::GetReferencers(UAsset* Asset, TArray<UAsset*>& OutReferencers) const
{
    OutReferencers.Reset();

    if (Asset == nullptr)
    {
        return;
    }

    for (const auto& Pair : LoadedAssets)
    {
        UAsset* Candidate = Pair.second;

        if (Candidate == nullptr || Candidate == Asset)
        {
            continue;
        }

        TArray<UAsset*> Dependencies;
        Candidate->GetDependencies(Dependencies);

        if (Dependencies.Contains(Asset))
        {
            OutReferencers.Add(Candidate);
        }
    }
}

void FAssetManager::RefreshAssetRegistry(const std::filesystem::path& InContentDirectory)
{
    AssetRegistry.Reset();

    const std::filesystem::path ContentDirectory = InContentDirectory.empty() ? std::filesystem::current_path() / "Content" : InContentDirectory;
    std::error_code DirectoryError;

    if (!std::filesystem::is_directory(ContentDirectory, DirectoryError) || DirectoryError)
    {
        return;
    }

    for (std::filesystem::recursive_directory_iterator It(ContentDirectory, std::filesystem::directory_options::skip_permission_denied, DirectoryError), End; !DirectoryError && It != End; It.increment(DirectoryError))
    {
        std::error_code EntryError;

        if (!It->is_regular_file(EntryError) || EntryError)
        {
            continue;
        }

        const std::string AssetType = GetAssetTypeFromExtension(PathToUTF8(It->path().extension()));

        if (AssetType.empty())
        {
            continue;
        }

        const std::string AssetPath = PathToUTF8(It->path().lexically_relative(std::filesystem::current_path()));

        if (!IsValidContentPath(AssetPath))
        {
            continue;
        }

        FAssetRegistryEntry Entry;
        Entry.AssetPath = AssetPath;
        Entry.AssetType = AssetType;
        Entry.FileSize = It->file_size(EntryError);

        if (EntryError)
        {
            Entry.FileSize = 0;
        }

        AssetRegistry.Add(Entry.AssetPath, std::move(Entry));
    }
}

void FAssetManager::GetAssetRegistryEntries(TArray<FAssetRegistryEntry>& OutEntries) const
{
    OutEntries.Reset();

    for (const auto& Pair : AssetRegistry)
    {
        OutEntries.Add(Pair.second);
    }

    OutEntries.Sort([](const FAssetRegistryEntry& A, const FAssetRegistryEntry& B)
    {
        return A.AssetPath < B.AssetPath;
    });
}

bool FAssetManager::FindAssetRegistryEntry(const std::string& AssetPath, FAssetRegistryEntry& OutEntry) const
{
    const FAssetRegistryEntry* Entry = AssetRegistry.Find(AssetPath);

    if (Entry == nullptr)
    {
        return false;
    }

    OutEntry = *Entry;
    return true;
}

void FAssetManager::Shutdown()
{
    for (auto& Pair : LoadedAssets)
    {
        delete Pair.second;
        Pair.second = nullptr;
    }

    LoadedAssets.Reset();
    AssetRegistry.Reset();
}
