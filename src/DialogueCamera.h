#pragma once

namespace ToggleDialogueCameraSF::DialogueCamera
{
    bool Install();
    bool IsOpen();
    [[nodiscard]] bool HandleMouseWheel(bool a_zoomIn);
    void Toggle();
}
