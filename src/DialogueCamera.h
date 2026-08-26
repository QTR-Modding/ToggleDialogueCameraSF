#pragma once

namespace RE
{
    class ThirdPersonState;
}

namespace ToggleDialogueCameraSF::DialogueCamera
{
    bool Install();
    bool IsOpen();
    [[nodiscard]] bool ShouldRouteDisabledMouseWheelThroughThirdPerson();
    [[nodiscard]] bool HandleDisabledMouseWheel(bool a_zoomIn);
    [[nodiscard]] bool HandleMouseWheel(bool a_zoomIn);
    [[nodiscard]] bool ShouldStopDisabledMouseWheelAfterThirdPerson(
        const RE::ThirdPersonState& a_state);
    [[nodiscard]] bool ShouldStopMouseWheelAfterThirdPerson(
        const RE::ThirdPersonState& a_state,
        bool a_zoomIn);
    void Toggle();
}
