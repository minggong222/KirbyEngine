#include "AssetManager.h"

#include <algorithm>
#include <cctype>
#include <system_error>
#include <vector>

FAssetManager::FAssetManager()
{
    RefreshAssetRegistry();
}

bool FAssetManager::IsValidContentPath(const std::string& AssetPath) const
{
    const std::filesystem::path Path = std::filesystem::u8path(AssetPath);

    if (Path.empty() || Path.is_absolute())
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

    return NormalizedPath.generic_string().starts_with("Content/");
}

std::string FAssetManager::GetAssetTypeFromExtension(const std::string& Extension)
{
    std::string LowerExtension = Extension;

    std::transform(LowerExtension.begin(), LowerExtension.end(), LowerExtension.begin(), [](unsigned char Character)
    {
        return static_cast<char>(std::tolower(Character));
    });

    if (LowerExtension == ".obj")
    {
        return "Static Mesh";
    }

    if (LowerExtension == ".kmat")
    {
        return "Material";
    }

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
    if (Asset == nullptr || !IsValidContentPath(NewAssetPath))
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

    for (const auto& Pair : LoadedAssets)
    {
        if (Pair.first == OldPrefix || Pair.first.starts_with(OldPrefix + "/"))
        {
            FRepathEntry Entry;
            Entry.OldPath = Pair.first;
            Entry.NewPath = NewPrefix + Pair.first.substr(OldPrefix.size());
            Entry.Asset = Pair.second;

            if (!IsValidContentPath(Entry.NewPath))
            {
                return false;
            }

            Entries.push_back(std::move(Entry));
        }
    }

    if (Entries.empty())
    {
        RefreshAssetRegistry();
        return true;
    }

    for (const FRepathEntry& Entry : Entries)
    {
        if (LoadedAssets.Contains(Entry.NewPath))
        {
            const bool bPathIsBeingMoved = std::any_of(Entries.begin(), Entries.end(), [&Entry](const FRepathEntry& Candidate)
            {
                return Candidate.OldPath == Entry.NewPath;
            });

            if (!bPathIsBeingMoved)
            {
                return false;
            }
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

        for (UAsset* Dependency : Dependencies)
        {
            if (Dependency == Asset)
            {
                OutReferencers.Add(Candidate);
                break;
            }
        }
    }
}

void FAssetManager::RefreshAssetRegistry(const std::filesystem::path& RootDirectory)
{
    AssetRegistry.Reset();

    const std::filesystem::path ContentDirectory = RootDirectory.empty() ? std::filesystem::current_path() / "Content" : RootDirectory;

    std::error_code DirectoryError;

    if (!std::filesystem::is_directory(ContentDirectory, DirectoryError) || DirectoryError)
    {
        return;
    }

    const std::filesystem::path ProjectDirectory = std::filesystem::current_path();

    for (std::filesystem::recursive_directory_iterator It(ContentDirectory, std::filesystem::directory_options::skip_permission_denied, DirectoryError), End; !DirectoryError && It != End; It.increment(DirectoryError))
    {
        std::error_code EntryError;

        if (!It->is_regular_file(EntryError) || EntryError)
        {
            continue;
        }

        const std::string AssetType = GetAssetTypeFromExtension(It->path().extension().string());

        if (AssetType.empty())
        {
            continue;
        }

        const std::filesystem::path RelativePath = std::filesystem::relative(It->path(), ProjectDirectory, EntryError);

        if (EntryError)
        {
            continue;
        }

        FAssetRegistryEntry Entry;
        Entry.AssetPath = RelativePath.generic_string();
        Entry.AssetType = AssetType;
        Entry.FileSize = It->file_size(EntryError);

        if (EntryError)
        {
            Entry.FileSize = 0;
        }

        AssetRegistry.Add(Entry.AssetPath, Entry);
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
