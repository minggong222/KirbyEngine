#include "AssetManager.h"

void FAssetManager::Shutdown()
{
    for (auto& Pair : LoadedAssets)
    {
        if (Pair.second != nullptr)
        {
            delete Pair.second;
            Pair.second = nullptr;
        }
    }
    LoadedAssets.Reset();
}
