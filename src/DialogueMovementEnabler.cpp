#include "DialogueMovementEnabler.h"

namespace ToggleDialogueCameraSF::DialogueMovementEnabler
{
	namespace
	{
		// Dialogue Movement Enabler 1.1.0, Remap::OnProcess and its vanilla chain target.
		constexpr std::uintptr_t kOnProcessRVA = 0x16EC0;
		constexpr std::uintptr_t kProcessChainRVA = 0xA1B08;
		constexpr std::array<std::uint8_t, 32> kExpectedOnProcess{
			0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83,
			0xEC, 0x20, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xF1,
			0xE8, 0x0B, 0xB6, 0xFF, 0xFF, 0xE8, 0xC6, 0xD9,
			0xFF, 0xFF, 0x84, 0xC0, 0x0F, 0x84, 0xB1, 0x00
		};

		struct ChainReference
		{
			std::uintptr_t rva;
			std::array<std::uint8_t, 7> bytes;
			std::size_t displacementOffset;
		};

		constexpr std::array kChainReferences{
			ChainReference{0x16F40, {0xFF, 0x15, 0xC2, 0xAB, 0x08, 0x00}, 2},
			ChainReference{0x16F50, {0x48, 0x8B, 0x1D, 0xB1, 0xAB, 0x08, 0x00}, 3},
			ChainReference{0x16FA3, {0x48, 0xFF, 0x25, 0x5E, 0xAB, 0x08, 0x00}, 3}
		};

		[[nodiscard]] bool Contains(
			const REX::FModuleSection& a_section,
			const std::uintptr_t a_address,
			const std::size_t a_size)
		{
			const auto start = a_section.GetAddress();
			const auto size = a_section.GetSize();
			return start != 0 && a_size <= size && a_address >= start &&
				a_address - start <= size - a_size;
		}
	}

	std::uintptr_t GetInputChainSlot(const std::uintptr_t a_currentInputHook)
	{
		const auto module = REX::FModule::GetLoadedModule("DialogueMovementEnabler.dll");
		const auto base = module.GetBaseAddress();
		if (base == 0)
		{
			return 0;
		}

		const auto text = module.GetSection(".text");
		if (!Contains(text, a_currentInputHook, 1))
		{
			return 0;
		}

		if (a_currentInputHook != base + kOnProcessRVA ||
			!Contains(text, a_currentInputHook, kExpectedOnProcess.size()) ||
			std::memcmp(
				reinterpret_cast<const void*>(a_currentInputHook),
				kExpectedOnProcess.data(),
				kExpectedOnProcess.size()) != 0)
		{
			logger::warn("Dialogue Movement Enabler input hook was not recognized.");
			return 0;
		}

		const auto slot = base + kProcessChainRVA;
		if (!Contains(module.GetSection(".data"), slot, sizeof(std::uintptr_t)))
		{
			logger::warn("Dialogue Movement Enabler input chain is outside its data section.");
			return 0;
		}

		for (const auto& reference : kChainReferences)
		{
			const auto instruction = base + reference.rva;
			const auto size = reference.displacementOffset + sizeof(std::int32_t);
			if (!Contains(text, instruction, size) ||
				std::memcmp(reinterpret_cast<const void*>(instruction), reference.bytes.data(), size) != 0)
			{
				logger::warn("Dialogue Movement Enabler input chain reference was not recognized.");
				return 0;
			}

			std::int32_t displacement;
			std::memcpy(
				&displacement,
				reinterpret_cast<const void*>(instruction + reference.displacementOffset),
				sizeof(displacement));
			if (instruction + size + displacement != slot)
			{
				logger::warn("Dialogue Movement Enabler input chain reference has an unexpected target.");
				return 0;
			}
		}

		if (*reinterpret_cast<const std::uintptr_t*>(slot) != REL::ID(82442).address())
		{
			logger::warn("Dialogue Movement Enabler input chain no longer points to vanilla PlayerControls.");
			return 0;
		}
		return slot;
	}
}
