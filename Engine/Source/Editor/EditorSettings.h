#pragma once

#include "Renderer/RenderSettings.h"

class FEditorSettings
{
public:
    bool Load(const char* FilePath);
    bool Save(const char* FilePath) const;

    EViewMode ViewMode = EViewMode::Lit;
    FEngineShowFlags ShowFlags;
    float CameraSensitivity = 0.05f;
    float GridExtent = 40.0f;
    float GridSpacing = 2.0f;
    int32 MajorGridInterval = 5;
    bool bSnapEnabled = false;
    float TranslationSnap = 1.0f;
    float RotationSnap = 15.0f;
    float ScaleSnap = 0.1f;
};
