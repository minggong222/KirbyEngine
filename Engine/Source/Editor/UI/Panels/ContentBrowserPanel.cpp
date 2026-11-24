#include "ContentBrowserPanel.h"

#include "imgui.h"

#include "Resources/AssetManager.h"
#include "Resources/StaticMesh.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"

#include "Actors/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "World/World.h"

bool FContentBrowserPanel::Init()
{
    return Context != nullptr && AssetManager != nullptr;
}

void FContentBrowserPanel::Tick(float DeltaTime)
{
}

void FContentBrowserPanel::OnRender()
{
    if (!IsOpen() || Context == nullptr || AssetManager == nullptr)
    {
        return;
    }

    bool bIsOpen = IsOpen();

    if (!ImGui::Begin("Content Browser", &bIsOpen))
    {
        SetOpen(bIsOpen);
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("AssetTabs"))
    {
        if (ImGui::BeginTabItem("Static Meshes"))
        {
            TArray<UStaticMesh*> Assets;
            AssetManager->GetAssets(Assets);

            for (UStaticMesh* Asset : Assets)
            {
                if (Asset == nullptr)
                {
                    continue;
                }

                const FString AssetPath = Asset->GetAssetPath();

                if (ImGui::Selectable(AssetPath.c_str()) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    if (AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Context->SelectedActor))
                    {
                        Actor->GetStaticMeshComponent()->SetStaticMesh(Asset);
                    }
                    else if (Context->World != nullptr)
                    {
                        AStaticMeshActor* NewActor = Context->World->SpawnActor<AStaticMeshActor>();

                        if (NewActor != nullptr)
                        {
                            NewActor->GetStaticMeshComponent()->SetStaticMesh(Asset);
                            Context->SelectActor(NewActor);
                        }
                    }
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Materials"))
        {
            TArray<UMaterial*> Assets;
            AssetManager->GetAssets(Assets);

            for (UMaterial* Asset : Assets)
            {
                if (Asset == nullptr)
                {
                    continue;
                }

                const FString AssetPath = Asset->GetAssetPath();

                if (ImGui::Selectable(AssetPath.c_str()) && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Context->SelectedActor);

                    if (Actor != nullptr && Actor->GetStaticMeshComponent() != nullptr)
                    {
                        Actor->GetStaticMeshComponent()->SetMaterial(Asset);
                    }
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Textures"))
        {
            TArray<UTexture2D*> Assets;
            AssetManager->GetAssets(Assets);

            for (UTexture2D* Asset : Assets)
            {
                if (Asset != nullptr)
                {
                    ImGui::Text("%s", Asset->GetAssetPath().c_str());
                }
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
    SetOpen(bIsOpen);
}