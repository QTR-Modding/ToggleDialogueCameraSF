#pragma once

namespace ToggleDialogueCameraSF::DialogueCamera
{
	bool Install();
	[[nodiscard]] bool IsTopInputMenu();
	[[nodiscard]] bool HandleDisabledMouseWheel(bool a_zoomIn);
	[[nodiscard]] bool HandleMouseWheel(bool a_zoomIn);
	[[nodiscard]] bool ShouldStopMouseWheelAfterThirdPerson(
		const RE::ThirdPersonState& a_state,
		bool a_zoomIn);
	void Toggle();
}
