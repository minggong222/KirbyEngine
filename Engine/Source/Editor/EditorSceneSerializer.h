#pragma once

#include <filesystem>

class FEditorObjectContext;
class FAssetManager;

class FSceneSerializer
{
public:
    static bool Save(const std::filesystem::path& FilePath, const FEditorObjectContext& Context);
    static bool Load(const std::filesystem::path& FilePath, FEditorObjectContext& Context, FAssetManager& AssetManager);
};