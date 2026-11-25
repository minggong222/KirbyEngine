#include "EditorSettings.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <cmath>
#include <algorithm>

bool FEditorSettings::Load(const char* FilePath)
{
    if (FilePath == nullptr)
    {
        return false;
    }

    std::ifstream Input(FilePath);

    if (!Input.is_open())
    {
        return false;
    }

    int32 SettingsVersion = 1;
    int32 LegacyStaticMeshes = ShowFlags.IsEnabled(EEngineShowFlag::Primitives) ? 1 : 0;
    int32 LegacyActorNames = ShowFlags.IsEnabled(EEngineShowFlag::BillboardText) ? 1 : 0;

    std::string Line;

    while (std::getline(Input, Line))
    {
        const std::size_t Separator = Line.find('=');

        if (Separator == std::string::npos)
        {
            continue;
        }

        const std::string Key = Line.substr(0, Separator);
        const std::string Value = Line.substr(Separator + 1);

        try
        {
            if (Key == "SettingsVersion")
            {
                SettingsVersion = std::stoi(Value);
            }
            else if (Key == "ViewMode")
            {
                const int32 Mode = std::stoi(Value);

                if (SettingsVersion >= 2)
                {
                    ViewMode = Mode >= 0 && Mode <= 2 ? static_cast<EViewMode>(Mode) : EViewMode::Lit;
                }
                else
                {
                    ViewMode = Mode == 1 ? EViewMode::Wireframe : EViewMode::Lit;
                }
            }
            else if (Key == "Primitives")
            {
                ShowFlags.SetEnabled(EEngineShowFlag::Primitives, std::stoi(Value) != 0);
            }
            else if (Key == "StaticMeshes")
            {
                LegacyStaticMeshes = std::stoi(Value) != 0;
            }
            else if (Key == "Grid")
            {
                ShowFlags.SetEnabled(EEngineShowFlag::Grid, std::stoi(Value) != 0);
            }
            else if (Key == "Axes")
            {
                ShowFlags.SetEnabled(EEngineShowFlag::Axes, std::stoi(Value) != 0);
            }
            else if (Key == "Bounds")
            {
                ShowFlags.SetEnabled(EEngineShowFlag::Bounds, std::stoi(Value) != 0);
            }
            else if (Key == "BillboardText")
            {
                ShowFlags.SetEnabled(EEngineShowFlag::BillboardText, std::stoi(Value) != 0);
            }
            else if (Key == "ActorNames")
            {
                LegacyActorNames = std::stoi(Value) != 0;
            }
            else if (Key == "CameraSensitivity")
            {
                CameraSensitivity = std::stof(Value);
            }
            else if (Key == "GridExtent")
            {
                GridExtent = std::stof(Value);
            }
            else if (Key == "GridSpacing")
            {
                GridSpacing = std::stof(Value);
            }
            else if (Key == "MajorGridInterval")
            {
                MajorGridInterval = std::stoi(Value);
            }
            else if (Key == "SnapEnabled")
            {
                bSnapEnabled = std::stoi(Value) != 0;
            }
            else if (Key == "TranslationSnap")
            {
                TranslationSnap = std::stof(Value);
            }
            else if (Key == "RotationSnap")
            {
                RotationSnap = std::stof(Value);
            }
            else if (Key == "ScaleSnap")
            {
                ScaleSnap = std::stof(Value);
            }
            else if (Key == "FourViewports")
            {
                bFourViewports = std::stoi(Value) != 0;
            }
            else if (Key == "ViewportSplitX")
            {
                ViewportSplitX = std::stof(Value);
            }
            else if (Key == "ViewportSplitY")
            {
                ViewportSplitY = std::stof(Value);
            }
        }
        catch (...)
        {
            continue;
        }
    }

    if (SettingsVersion < 2)
    {
        ShowFlags.SetEnabled(EEngineShowFlag::Primitives, LegacyStaticMeshes != 0);
        ShowFlags.SetEnabled(EEngineShowFlag::BillboardText, LegacyActorNames != 0);
    }

    CameraSensitivity = std::isfinite(CameraSensitivity) ? std::clamp(CameraSensitivity, 0.001f, 1.0f) : 0.05f;
    GridExtent = std::isfinite(GridExtent) ? std::clamp(GridExtent, 1.0f, 10000.0f) : 40.0f;
    GridSpacing = std::isfinite(GridSpacing) ? std::clamp(GridSpacing, 0.1f, 1000.0f) : 2.0f;
    MajorGridInterval = std::clamp(MajorGridInterval, 1, 100);
    TranslationSnap = std::isfinite(TranslationSnap) ? std::clamp(TranslationSnap, 0.001f, 1000.0f) : 1.0f;
    RotationSnap = std::isfinite(RotationSnap) ? std::clamp(RotationSnap, 0.1f, 180.0f) : 15.0f;
    ScaleSnap = std::isfinite(ScaleSnap) ? std::clamp(ScaleSnap, 0.001f, 100.0f) : 0.1f;

    ViewportSplitX = std::isfinite(ViewportSplitX) ? std::clamp(ViewportSplitX, 0.2f, 0.8f) : 0.5f;
    ViewportSplitY = std::isfinite(ViewportSplitY) ? std::clamp(ViewportSplitY, 0.2f, 0.8f) : 0.5f;

    return true;
}

bool FEditorSettings::Save(const char* FilePath) const
{
    if (FilePath == nullptr)
    {
        return false;
    }

    const std::filesystem::path Path(FilePath);

    std::error_code Error;

    if (!Path.parent_path().empty())
    {
        std::filesystem::create_directories(Path.parent_path(), Error);

        if (Error)
        {
            return false;
        }
    }

    std::ofstream Output(Path);

    if (!Output.is_open())
    {
        return false;
    }

    const float SafeSplitX = std::isfinite(ViewportSplitX) ? std::clamp(ViewportSplitX, 0.2f, 0.8f) : 0.5f;
    const float SafeSplitY = std::isfinite(ViewportSplitY) ? std::clamp(ViewportSplitY, 0.2f, 0.8f) : 0.5f;

    Output << "SettingsVersion=3\n";
    Output << "ViewMode=" << static_cast<int32>(ViewMode) << '\n';
    Output << "Primitives=" << ShowFlags.IsEnabled(EEngineShowFlag::Primitives) << '\n';
    Output << "Grid=" << ShowFlags.IsEnabled(EEngineShowFlag::Grid) << '\n';
    Output << "Axes=" << ShowFlags.IsEnabled(EEngineShowFlag::Axes) << '\n';
    Output << "Bounds=" << ShowFlags.IsEnabled(EEngineShowFlag::Bounds) << '\n';
    Output << "BillboardText=" << ShowFlags.IsEnabled(EEngineShowFlag::BillboardText) << '\n';
    Output << "CameraSensitivity=" << CameraSensitivity << '\n';
    Output << "GridExtent=" << GridExtent << '\n';
    Output << "GridSpacing=" << GridSpacing << '\n';
    Output << "MajorGridInterval=" << MajorGridInterval << '\n';
    Output << "SnapEnabled=" << bSnapEnabled << '\n';
    Output << "TranslationSnap=" << TranslationSnap << '\n';
    Output << "RotationSnap=" << RotationSnap << '\n';
    Output << "ScaleSnap=" << ScaleSnap << '\n';
    Output << "FourViewports=" << bFourViewports << '\n';
    Output << "ViewportSplitX=" << SafeSplitX << '\n';
    Output << "ViewportSplitY=" << SafeSplitY << '\n';

    return Output.good();
}