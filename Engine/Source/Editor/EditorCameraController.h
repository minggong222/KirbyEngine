#pragma once

class UCameraComponent;
class FWindowsWindow;

class FEditorCameraController
{
public:
    void Update(UCameraComponent& Camera, FWindowsWindow& Window, float DeltaTime);
    void Stop(FWindowsWindow& Window);
    
private:
    bool bCameraLookActive = false;
    
};
