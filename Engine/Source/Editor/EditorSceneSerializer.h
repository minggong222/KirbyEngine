#pragma once

#include <filesystem>

struct FEditorObjectContext;
class FAssetManager;
class UCameraComponent;

class FSceneSerializer
{
public:
    static bool Save(const std::filesystem::path& FilePath, const FEditorObjectContext& Context, const UCameraComponent& ViewportCamera);
    static bool Load(const std::filesystem::path& FilePath, FEditorObjectContext& Context, FAssetManager& AssetManager, UCameraComponent& ViewportCamera);
};