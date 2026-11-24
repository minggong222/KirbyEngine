#include "ContentBrowserPanel.h"

#include "imgui.h"

#include "Resources/AssetManager.h"
#include "Resources/StaticMesh.h"
#include "Resources/Material.h"
#include "Resources/Texture2D.h"
#include "EditorUndoManager.h"
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
                    UStaticMeshComponent* TargetComponent = Cast<UStaticMeshComponent>(Context->SelectedComponent);

                    if (TargetComponent == nullptr)
                    {
                        if (AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Context->SelectedActor))
                        {
                            TargetComponent = Actor->GetStaticMeshComponent();
                        }
                    }

                    if (TargetComponent != nullptr)
                    {
                        UStaticMesh* PreviousMesh = TargetComponent->GetStaticMesh();
                        TargetComponent->SetStaticMesh(Asset);

                        if (Context->UndoManager != nullptr)
                        {
                            Context->UndoManager->RecordStaticMeshChange(*Context, TargetComponent, PreviousMesh, Asset);
                        }
                    }
                    else if (Context->World != nullptr)
                    {
                        const uint32 PreviousSelectedUUID = Context->GetSelectedObject() != nullptr ? Context->GetSelectedObject()->GetUUID() : 0;

                        AStaticMeshActor* NewActor = Context->World->SpawnActor<AStaticMeshActor>(FName("StaticMeshActor"));

                        if (NewActor != nullptr && NewActor->GetStaticMeshComponent() != nullptr)
                        {
                            NewActor->GetStaticMeshComponent()->SetStaticMesh(Asset);
                            Context->SelectActor(NewActor);

                            if (Context->UndoManager != nullptr)
                            {
                                Context->UndoManager->RecordCreate(*Context, NewActor, PreviousSelectedUUID);
                            }
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
                    UStaticMeshComponent* Component = Cast<UStaticMeshComponent>(Context->SelectedComponent);

                    if (Component == nullptr)
                    {
                        if (AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Context->SelectedActor))
                        {
                            Component = Actor->GetStaticMeshComponent();
                        }
                    }

                    if (Component != nullptr)
                    {
                        UMaterial* PreviousMaterial = Component->GetMaterial();

                        Component->SetMaterial(Asset);

                        if (Context->UndoManager != nullptr)
                        {
                            Context->UndoManager->RecordMaterialChange(*Context, Component, PreviousMaterial, Asset);
                        }
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