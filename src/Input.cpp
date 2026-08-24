#include "Input.h"
#include "DialogueCamera.h"
#include "Settings.h"
#include "REX/FModule.h"

#include <array>
#include <cstring>
#include <string_view>
#include <utility>

namespace ToggleDialogueCameraSF::Input
{
    namespace
    {
        constexpr std::string_view kTogglePOVEvent{ "TogglePOV" };
        constexpr std::string_view kZoomInEvent{ "ZoomIn" };
        constexpr std::string_view kZoomOutEvent{ "ZoomOut" };
        constexpr std::int32_t kMouseWheelUp{ 8 };
        constexpr std::int32_t kMouseWheelDown{ 9 };

        // PlayerCamera's BSInputEventReceiver subobject for Starfield 1.16.244.
        // This receiver owns the camera-state dispatch, so consuming here precedes vanilla camera handling.
        constexpr std::size_t kPlayerCameraInputVtable = 5;
        constexpr std::size_t kPerformInputProcessingSlot = 1;
        constexpr std::size_t kMaximumQueueLength = 512;
        constexpr std::array<std::uint8_t, 16> kExpectedPlayerCameraInputPrologue{
            0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x41, 0xC8, 0x48, 0x8B
        };

        using InputProcessor = void (*)(RE::BSInputEventReceiver*, const RE::InputEvent*);
        InputProcessor originalPlayerCameraInput{ nullptr };

        [[nodiscard]] bool HasExpectedPrologue(const std::uintptr_t a_address)
        {
            const auto text = REX::FModule::GetExecutingModule().GetSection(".text");
            const auto textAddress = text.GetAddress();
            const auto textSize = text.GetSize();
            if (a_address == 0 || textAddress == 0 || kExpectedPlayerCameraInputPrologue.size() > textSize ||
                a_address < textAddress || a_address - textAddress > textSize - kExpectedPlayerCameraInputPrologue.size()) {
                return false;
            }

            return std::memcmp(
                       reinterpret_cast<const void*>(a_address),
                       kExpectedPlayerCameraInputPrologue.data(),
                       kExpectedPlayerCameraInputPrologue.size()) == 0;
        }

        [[nodiscard]] bool IsToggleInput(const RE::ButtonEvent& a_button)
        {
            const auto userEvent = std::string_view{ a_button.QUserEvent().c_str() };
            if (userEvent == kTogglePOVEvent || userEvent == kZoomInEvent || userEvent == kZoomOutEvent) {
                return true;
            }

            const auto& config = Settings::Get();
            if (a_button.deviceType == RE::InputEvent::DeviceType::kKeyboard) {
                return config.keyboardToggleKey >= 0 && a_button.idCode == config.keyboardToggleKey;
            }
            if (a_button.deviceType == RE::InputEvent::DeviceType::kMouse) {
                return a_button.idCode == kMouseWheelUp || a_button.idCode == kMouseWheelDown;
            }
            if (a_button.deviceType == RE::InputEvent::DeviceType::kGamepad) {
                return config.gamepadToggleKey >= 0 && a_button.idCode == config.gamepadToggleKey;
            }
            return false;
        }

        void ConsumeToggleInput(const RE::ButtonEvent* a_button)
        {
            if (!a_button || !IsToggleInput(*a_button)) {
                return;
            }

            auto* const mutableButton = const_cast<RE::ButtonEvent*>(a_button);
            const auto previousStatus = mutableButton->status;
            mutableButton->status = RE::InputEvent::Status::kStop;

            const bool initialPress = a_button->value != 0.0F && a_button->heldDownSecs == 0.0F;
            if (initialPress) {
                logger::info(
                    "Toggle input accepted: device={}, id={}, event={}, time={}, prior status={}, value={}, held={}.",
                    std::to_underlying(a_button->deviceType),
                    a_button->idCode,
                    a_button->QUserEvent().c_str(),
                    a_button->timeCode,
                    std::to_underlying(previousStatus),
                    a_button->value,
                    a_button->heldDownSecs);
                DialogueCamera::Toggle();
            } else {
                logger::debug(
                    "Toggle continuation stopped: device={}, id={}, event={}, time={}, prior status={}, value={}, held={}.",
                    std::to_underlying(a_button->deviceType),
                    a_button->idCode,
                    a_button->QUserEvent().c_str(),
                    a_button->timeCode,
                    std::to_underlying(previousStatus),
                    a_button->value,
                    a_button->heldDownSecs);
            }
        }

        void ProcessPlayerCameraInput(RE::BSInputEventReceiver* a_receiver, const RE::InputEvent* a_queueHead)
        {
            if (DialogueCamera::IsOpen()) {
                auto event = a_queueHead;
                std::size_t eventCount = 0;
                while (event && eventCount < kMaximumQueueLength) {
                    if (event->eventType == RE::InputEvent::EventType::kButton) {
                        ConsumeToggleInput(static_cast<const RE::ButtonEvent*>(event));
                    }
                    event = event->next;
                    ++eventCount;
                }

                if (event) {
                    logger::error(
                        "PlayerCamera input queue exceeded {} events; remaining events were not inspected.",
                        kMaximumQueueLength);
                }
            }

            originalPlayerCameraInput(a_receiver, a_queueHead);
        }
    }

    bool Install()
    {
        REL::Relocation<std::uintptr_t> playerCameraInputVtable{
            RE::VTABLE::PlayerCamera[kPlayerCameraInputVtable]
        };
        const auto slotAddress =
            playerCameraInputVtable.address() + sizeof(std::uintptr_t) * kPerformInputProcessingSlot;
        const auto originalAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);

        if (!HasExpectedPrologue(originalAddress)) {
            logger::error("PlayerCamera input receiver preflight failed; the input hook was not installed.");
            return false;
        }

        const auto hookAddress = REX::UNRESTRICTED_CAST<std::uintptr_t>(ProcessPlayerCameraInput);
        const auto replacedAddress =
            playerCameraInputVtable.write_vfunc(kPerformInputProcessingSlot, hookAddress);
        originalPlayerCameraInput = REX::UNRESTRICTED_CAST<InputProcessor>(replacedAddress);

        const bool originalMatched = replacedAddress == originalAddress;
        const bool hookMatched = *reinterpret_cast<const std::uintptr_t*>(slotAddress) == hookAddress;
        if (!originalMatched || !hookMatched) {
            const auto restoreAddress = replacedAddress != 0 ? replacedAddress : originalAddress;
            playerCameraInputVtable.write_vfunc(kPerformInputProcessingSlot, restoreAddress);
            const bool restored = *reinterpret_cast<const std::uintptr_t*>(slotAddress) == restoreAddress;
            originalPlayerCameraInput = nullptr;
            logger::error(
                "PlayerCamera input hook verification failed: original matched={}, hook readback={}, rollback={}.",
                originalMatched,
                hookMatched,
                restored);
            return false;
        }

        logger::info("Installed PlayerCamera PerformInputProcessing hook at {:X}.", originalAddress);
        return true;
    }
}
