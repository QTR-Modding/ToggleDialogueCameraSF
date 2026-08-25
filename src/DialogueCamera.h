#pragma once

namespace RE
{
    class ThirdPersonState;
}

namespace ToggleDialogueCameraSF::DialogueCamera
{
    bool Install();
    bool IsOpen();
    [[nodiscard]] bool HandleMouseWheel(bool a_zoomIn);
    [[nodiscard]] bool ShouldStopMouseWheelAfterThirdPerson(
        const RE::ThirdPersonState& a_state,
        bool a_zoomIn);
    void Toggle();
}
