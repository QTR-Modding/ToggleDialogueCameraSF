#pragma once

namespace ToggleDialogueCameraSF::DialogueMovementEnabler
{
	struct InputHook
	{
		std::uintptr_t chainSlot{0};
		bool processCameraFirst{false};
	};

	[[nodiscard]] InputHook GetInputHook(std::uintptr_t a_currentInputHook);
}
