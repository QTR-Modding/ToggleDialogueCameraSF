#include "DialogueMovementEnabler.h"

namespace ToggleDialogueCameraSF::DialogueMovementEnabler
{
	namespace
	{
		struct ChainReference
		{
			std::uintptr_t rva;
			std::array<std::uint8_t, 7> bytes;
			std::size_t displacementOffset;
		};

		constexpr std::array kPublishedChainReferences{
			ChainReference{0x16F40, {0xFF, 0x15, 0xC2, 0xAB, 0x08, 0x00}, 2},
			ChainReference{0x16F50, {0x48, 0x8B, 0x1D, 0xB1, 0xAB, 0x08, 0x00}, 3},
			ChainReference{0x16FA3, {0x48, 0xFF, 0x25, 0x5E, 0xAB, 0x08, 0x00}, 3}
		};
		constexpr std::array kLiveCameraChainReferences{
			ChainReference{0x16FED, {0xFF, 0x15, 0x85, 0xF8, 0x07, 0x00}, 2},
			ChainReference{0x16FFD, {0x48, 0x8B, 0x1D, 0x74, 0xF8, 0x07, 0x00}, 3},
			ChainReference{0x17050, {0x48, 0xFF, 0x25, 0x21, 0xF8, 0x07, 0x00}, 3}
		};

		struct SupportedBuild
		{
			std::uintptr_t onProcessRVA;
			std::uintptr_t processChainRVA;
			std::array<std::uint8_t, 32> entryBytes;
			std::span<const ChainReference> chainReferences;
			bool processCameraFirst;
		};

		constexpr std::array kSupportedBuilds{
			SupportedBuild{0x16EC0, 0xA1B08, {
				0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83,
				0xEC, 0x20, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xF1,
				0xE8, 0x0B, 0xB6, 0xFF, 0xFF, 0xE8, 0xC6, 0xD9,
				0xFF, 0xFF, 0x84, 0xC0, 0x0F, 0x84, 0xB1, 0x00
			}, kPublishedChainReferences, false},
			SupportedBuild{0x16F60, 0x96878, {
				0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83,
				0xEC, 0x20, 0x48, 0x8B, 0xFA, 0x48, 0x8B, 0xF1,
				0xE8, 0x8B, 0xB8, 0xFF, 0xFF, 0xE8, 0xF6, 0xDA,
				0xFF, 0xFF, 0x84, 0xC0, 0x0F, 0x84, 0xBE, 0x00
			}, kLiveCameraChainReferences, true}
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

	InputHook GetInputHook(const std::uintptr_t a_currentInputHook)
	{
		const auto module = REX::FModule::GetLoadedModule("DialogueMovementEnabler.dll");
		const auto base = module.GetBaseAddress();
		if (base == 0)
		{
			return {};
		}

		const auto text = module.GetSection(".text");
		if (!Contains(text, a_currentInputHook, 1))
		{
			return {};
		}

		const SupportedBuild* build = nullptr;
		for (const auto& candidate : kSupportedBuilds)
		{
			if (a_currentInputHook == base + candidate.onProcessRVA &&
				Contains(text, a_currentInputHook, candidate.entryBytes.size()) &&
				std::memcmp(reinterpret_cast<const void*>(a_currentInputHook),
					candidate.entryBytes.data(), candidate.entryBytes.size()) == 0)
			{
				build = &candidate;
				break;
			}
		}
		if (!build)
		{
			logger::warn("Dialogue Movement Enabler input hook was not recognized.");
			return {};
		}

		const auto slot = base + build->processChainRVA;
		if (!Contains(module.GetSection(".data"), slot, sizeof(std::uintptr_t)))
		{
			logger::warn("Dialogue Movement Enabler input chain is outside its data section.");
			return {};
		}

		for (const auto& reference : build->chainReferences)
		{
			const auto instruction = base + reference.rva;
			const auto size = reference.displacementOffset + sizeof(std::int32_t);
			if (!Contains(text, instruction, size) ||
				std::memcmp(reinterpret_cast<const void*>(instruction), reference.bytes.data(), size) != 0)
			{
				logger::warn("Dialogue Movement Enabler input chain reference was not recognized.");
				return {};
			}

			std::int32_t displacement;
			std::memcpy(
				&displacement,
				reinterpret_cast<const void*>(instruction + reference.displacementOffset),
				sizeof(displacement));
			if (instruction + size + displacement != slot)
			{
				logger::warn("Dialogue Movement Enabler input chain reference has an unexpected target.");
				return {};
			}
		}

		if (*reinterpret_cast<const std::uintptr_t*>(slot) != REL::ID(82442).address())
		{
			logger::warn("Dialogue Movement Enabler input chain no longer points to vanilla PlayerControls.");
			return {};
		}
		return {slot, build->processCameraFirst};
	}
}
