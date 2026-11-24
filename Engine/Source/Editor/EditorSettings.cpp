#include "EditorSettings.h"

#include <filesystem>
#include <fstream>
#include <string>

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
            if (Key == "ViewMode")
            {
                const int32 Mode = std::stoi(Value);
                ViewMode = Mode == 1 ? EViewMode::Wireframe : EViewMode::Lit;
            }
            else if (Key == "StaticMeshes") ShowFlags.StaticMeshes = std::stoi(Value) != 0;
            else if (Key == "Grid") ShowFlags.Grid = std::stoi(Value) != 0;
            else if (Key == "Axes") ShowFlags.Axes = std::stoi(Value) != 0;
            else if (Key == "Bounds") ShowFlags.Bounds = std::stoi(Value) != 0;
            else if (Key == "Billboards") ShowFlags.Billboards = std::stoi(Value) != 0;
            else if (Key == "ActorNames") ShowFlags.ActorNames = std::stoi(Value) != 0;
            else if (Key == "GridExtent") GridExtent = std::stof(Value);
            else if (Key == "GridSpacing") GridSpacing = std::stof(Value);
            else if (Key == "MajorGridInterval") MajorGridInterval = std::stoi(Value);
            else if (Key == "SnapEnabled") bSnapEnabled = std::stoi(Value) != 0;
            else if (Key == "TranslationSnap") TranslationSnap = std::stof(Value);
            else if (Key == "RotationSnap") RotationSnap = std::stof(Value);
            else if (Key == "ScaleSnap") ScaleSnap = std::stof(Value);
        }
        catch (...)
        {
            continue;
        }
    }

    GridExtent = std::clamp(GridExtent, 1.0f, 10000.0f);
    GridSpacing = std::clamp(GridSpacing, 0.1f, 1000.0f);
    MajorGridInterval = std::clamp(MajorGridInterval, 1, 100);
    TranslationSnap = std::clamp(TranslationSnap, 0.001f, 1000.0f);
    RotationSnap = std::clamp(RotationSnap, 0.1f, 180.0f);
    ScaleSnap = std::clamp(ScaleSnap, 0.001f, 100.0f);

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
    std::filesystem::create_directories(Path.parent_path(), Error);

    if (Error)
    {
        return false;
    }

    std::ofstream Output(Path);

    if (!Output.is_open())
    {
        return false;
    }

    Output << "ViewMode=" << static_cast<int32>(ViewMode) << '\n';
    Output << "StaticMeshes=" << ShowFlags.StaticMeshes << '\n';
    Output << "Grid=" << ShowFlags.Grid << '\n';
    Output << "Axes=" << ShowFlags.Axes << '\n';
    Output << "Bounds=" << ShowFlags.Bounds << '\n';
    Output << "Billboards=" << ShowFlags.Billboards << '\n';
    Output << "ActorNames=" << ShowFlags.ActorNames << '\n';

    Output << "GridExtent=" << GridExtent << '\n';
    Output << "GridSpacing=" << GridSpacing << '\n';
    Output << "MajorGridInterval=" << MajorGridInterval << '\n';

    Output << "SnapEnabled=" << bSnapEnabled << '\n';
    Output << "TranslationSnap=" << TranslationSnap << '\n';
    Output << "RotationSnap=" << RotationSnap << '\n';
    Output << "ScaleSnap=" << ScaleSnap << '\n';

    return Output.good();
}
