#include "Input.h"
#include "DialogueCamera.h"
#include "Settings.h"

namespace ToggleDialogueCameraSF::Input
{
    namespace
    {
        constexpr std::string_view kTogglePOVEvent{ "TogglePOV" };
        constexpr std::string_view kZoomInEvent{ "ZoomIn" };
        constexpr std::string_view kZoomOutEvent{ "ZoomOut" };
        constexpr std::int32_t kMouseWheelUp{ 0x800 };
        constexpr std::int32_t kMouseWheelDown{ 0x900 };

        // PlayerControls::Manager is the first normal receiver on Starfield 1.16.244.
        // Toggle keys are consumed there before vanilla can claim them.
        constexpr std::size_t kPlayerControlsInputVtable = 8;
        constexpr std::size_t kUIInputVtable = 10;
        constexpr std::size_t kPerformInputProcessingSlot = 1;
        constexpr std::size_t kMaximumQueueLength = 512;
        constexpr std::array<std::uint8_t, 16> kExpectedPlayerControlsInputPrologue{
            0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x20
        };
        constexpr std::array<std::uint8_t, 16> kExpectedUIInputPrologue{
            0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20, 0x55
        };

        using InputProcessor = void (*)(RE::BSInputEventReceiver*, const RE::InputEvent*);
        InputProcessor originalPlayerControlsInput{ nullptr };
        InputProcessor originalUIInput{ nullptr };

        enum class MouseWheelDirection : std::uint8_t
        {
            kNone,
            kIn,
            kOut
        };

        template <std::size_t N>
        [[nodiscard]] bool HasExpectedPrologue(
            const std::uintptr_t a_address,
            const std::array<std::uint8_t, N>& a_expected)
        {
            const auto text = REX::FModule::GetExecutingModule().GetSection(".text");
            const auto textAddress = text.GetAddress();
            const auto textSize = text.GetSize();
            if (a_address == 0 || textAddress == 0 || a_expected.size() > textSize ||
                a_address < textAddress || a_address - textAddress > textSize - a_expected.size()) {
                return false;
            }

            return std::memcmp(
                       reinterpret_cast<const void*>(a_address),
                       a_expected.data(),
                       a_expected.size()) == 0;
        }

        [[nodiscard]] MouseWheelDirection GetMouseWheelDirection(const RE::ButtonEvent& a_button)
        {
            if (a_button.deviceType != RE::InputEvent::DeviceType::kMouse) {
                return MouseWheelDirection::kNone;
            }
            if (a_button.idCode == kMouseWheelUp) {
                return MouseWheelDirection::kIn;
            }
            if (a_button.idCode == kMouseWheelDown) {
                return MouseWheelDirection::kOut;
            }

            const auto userEvent = std::string_view{ a_button.QUserEvent().c_str() };
            if (userEvent == kZoomInEvent) {
                return MouseWheelDirection::kIn;
            }
            if (userEvent == kZoomOutEvent) {
                return MouseWheelDirection::kOut;
            }
            return MouseWheelDirection::kNone;
        }

        [[nodiscard]] bool IsConfiguredToggleInput(const RE::ButtonEvent& a_button)
        {
            const auto& config = Settings::Get();
            if (a_button.deviceType == RE::InputEvent::DeviceType::kKeyboard) {
                return config.keyboardToggleKey >= 0 && a_button.idCode == config.keyboardToggleKey;
            }
            if (a_button.deviceType == RE::InputEvent::DeviceType::kGamepad) {
                return config.gamepadToggleKey >= 0 && a_button.idCode == config.gamepadToggleKey;
            }
            return false;
        }

        [[nodiscard]] bool IsToggleInput(const RE::ButtonEvent& a_button)
        {
            if (a_button.deviceType != RE::InputEvent::DeviceType::kKeyboard &&
                a_button.deviceType != RE::InputEvent::DeviceType::kGamepad) {
                return false;
            }
            return IsConfiguredToggleInput(a_button) ||
                   std::string_view{ a_button.QUserEvent().c_str() } == kTogglePOVEvent;
        }

        void LogKeyboardToggleCandidate(const RE::ButtonEvent& a_button)
        {
            if (a_button.deviceType != RE::InputEvent::DeviceType::kKeyboard || !IsToggleInput(a_button)) {
                return;
            }

            logger::info(
                "Keyboard toggle candidate observed before PlayerControls: "
                "id={}, event={}, time={}, status={}, value={}, held={}.",
                a_button.idCode,
                a_button.QUserEvent().c_str(),
                a_button.timeCode,
                std::to_underlying(a_button.status),
                a_button.value,
                a_button.heldDownSecs);
        }

        [[nodiscard]] bool ConsumeToggleInput(
            const RE::ButtonEvent* a_button,
            const bool a_toggleAlreadyRequested)
        {
            if (!a_button || a_button->status == RE::InputEvent::Status::kStop || !IsToggleInput(*a_button)) {
                return false;
            }

            auto* const mutableButton = const_cast<RE::ButtonEvent*>(a_button);
            const auto previousStatus = mutableButton->status;
            mutableButton->status = RE::InputEvent::Status::kStop;

            const bool initialPress = a_button->value != 0.0F && a_button->heldDownSecs == 0.0F;
            if (!initialPress) {
                logger::debug(
                    "Toggle continuation stopped: device={}, id={}, event={}, time={}, prior status={}, value={}, held={}.",
                    std::to_underlying(a_button->deviceType),
                    a_button->idCode,
                    a_button->QUserEvent().c_str(),
                    a_button->timeCode,
                    std::to_underlying(previousStatus),
                    a_button->value,
                    a_button->heldDownSecs);
                return false;
            }

            if (a_toggleAlreadyRequested) {
                logger::debug(
                    "Additional toggle press in the same PlayerControls input queue was stopped: device={}, id={}, event={}.",
                    std::to_underlying(a_button->deviceType),
                    a_button->idCode,
                    a_button->QUserEvent().c_str());
                return false;
            }

            logger::info(
                "Toggle input accepted before PlayerControls: device={}, id={}, event={}, time={}, prior status={}, value={}, held={}.",
                std::to_underlying(a_button->deviceType),
                a_button->idCode,
                a_button->QUserEvent().c_str(),
                a_button->timeCode,
                std::to_underlying(previousStatus),
                a_button->value,
                a_button->heldDownSecs);
            return true;
        }

        void ProcessPlayerControlsInput(RE::BSInputEventReceiver* a_receiver, const RE::InputEvent* a_queueHead)
        {
            if (DialogueCamera::IsOpen()) {
                auto event = a_queueHead;
                std::size_t eventCount = 0;
                bool toggleRequested = false;
                bool cycleThirdPersonDistance = false;
                while (event && eventCount < kMaximumQueueLength) {
                    if (event->eventType == RE::InputEvent::EventType::kButton) {
                        auto const button = static_cast<const RE::ButtonEvent*>(event);
                        LogKeyboardToggleCandidate(*button);
                        if (ConsumeToggleInput(button, toggleRequested)) {
                            toggleRequested = true;
                            cycleThirdPersonDistance =
                                button->deviceType == RE::InputEvent::DeviceType::kGamepad &&
                                IsConfiguredToggleInput(*button);
                        }
                    }
                    event = event->next;
                    ++eventCount;
                }

                if (event) {
                    logger::error(
                        "PlayerControls input queue exceeded {} events; remaining events were not inspected.",
                        kMaximumQueueLength);
                }
                if (toggleRequested) {
                    DialogueCamera::Toggle(cycleThirdPersonDistance);
                }
            }

            originalPlayerControlsInput(a_receiver, a_queueHead);
        }

        void ProcessUIInput(RE::BSInputEventReceiver* a_receiver, const RE::InputEvent* a_queueHead)
        {
            if (DialogueCamera::IsOpen()) {
                auto event = a_queueHead;
                std::size_t eventCount = 0;
                const RE::ButtonEvent* lastMouseWheelInput = nullptr;
                while (event && eventCount < kMaximumQueueLength) {
                    if (event->eventType == RE::InputEvent::EventType::kButton) {
                        auto const button = static_cast<const RE::ButtonEvent*>(event);
                        if (button->status != RE::InputEvent::Status::kStop &&
                            button->value != 0.0F && button->heldDownSecs == 0.0F &&
                            GetMouseWheelDirection(*button) != MouseWheelDirection::kNone) {
                            lastMouseWheelInput = button;
                        }
                    }
                    event = event->next;
                    ++eventCount;
                }

                if (event) {
                    logger::error(
                        "UI input queue exceeded {} events; remaining events were not inspected.",
                        kMaximumQueueLength);
                }

                if (lastMouseWheelInput) {
                    const auto direction = GetMouseWheelDirection(*lastMouseWheelInput);
                    if (DialogueCamera::HandleMouseWheel(direction == MouseWheelDirection::kIn)) {
                        logger::info(
                            "Mouse zoom boundary matched: direction={}, id={}, event={}, time={}, status preserved={}.",
                            direction == MouseWheelDirection::kIn ? "in" : "out",
                            lastMouseWheelInput->idCode,
                            lastMouseWheelInput->QUserEvent().c_str(),
                            lastMouseWheelInput->timeCode,
                            std::to_underlying(lastMouseWheelInput->status));
                    }
                }
            }

            originalUIInput(a_receiver, a_queueHead);
        }

        template <std::size_t N>
        [[nodiscard]] bool InstallVtableHook(
            REL::Relocation<std::uintptr_t>& a_vtable,
            const std::size_t a_slot,
            const std::uintptr_t a_hookAddress,
            const std::array<std::uint8_t, N>& a_expectedPrologue,
            InputProcessor& a_original,
            const std::string_view a_name)
        {
            const auto slotAddress = a_vtable.address() + sizeof(std::uintptr_t) * a_slot;
            const auto originalAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
            if (!HasExpectedPrologue(originalAddress, a_expectedPrologue)) {
                logger::error("{} preflight failed; the input hook was not installed.", a_name);
                return false;
            }

            a_original = REX::UNRESTRICTED_CAST<InputProcessor>(originalAddress);
            const auto replacedAddress = a_vtable.write_vfunc(a_slot, a_hookAddress);
            const auto chainAddress = replacedAddress != 0 ? replacedAddress : originalAddress;
            a_original = REX::UNRESTRICTED_CAST<InputProcessor>(chainAddress);

            const bool originalMatched = replacedAddress == originalAddress;
            auto liveAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
            const bool hookMatched = liveAddress == a_hookAddress;
            if (!originalMatched || !hookMatched) {
                const bool rollbackAttempted = liveAddress == a_hookAddress;
                if (rollbackAttempted) {
                    a_vtable.write_vfunc(a_slot, chainAddress);
                    liveAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
                }

                const bool rollbackVerified = rollbackAttempted && liveAddress == chainAddress;
                const bool hookStillDirectlyInstalled = liveAddress == a_hookAddress;
                const bool hookMayRemainChained = !rollbackAttempted && liveAddress != chainAddress;
                logger::error(
                    "{} verification failed: original matched={}, hook readback={}, "
                    "rollback attempted={}, rollback verified={}, hook directly installed={}, "
                    "hook may remain chained={}.",
                    a_name,
                    originalMatched,
                    hookMatched,
                    rollbackAttempted,
                    rollbackVerified,
                    hookStillDirectlyInstalled,
                    hookMayRemainChained);
                return false;
            }

            logger::info("Installed {} at {:X}.", a_name, originalAddress);
            return true;
        }

        [[nodiscard]] bool RestoreVtableHook(
            REL::Relocation<std::uintptr_t>& a_vtable,
            const std::size_t a_slot,
            const std::uintptr_t a_hookAddress,
            InputProcessor& a_original,
            const std::string_view a_name)
        {
            const auto slotAddress = a_vtable.address() + sizeof(std::uintptr_t) * a_slot;
            auto liveAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
            if (liveAddress != a_hookAddress) {
                logger::critical(
                    "{} rollback could not safely proceed because the live slot changed; "
                    "the original chain pointer was retained in case this hook remains reachable.",
                    a_name);
                return false;
            }

            const auto chainAddress = REX::UNRESTRICTED_CAST<std::uintptr_t>(a_original);
            if (chainAddress == 0) {
                logger::critical("{} rollback has no valid chain target while the hook is live.", a_name);
                return false;
            }

            a_vtable.write_vfunc(a_slot, chainAddress);
            liveAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
            const bool restored = liveAddress == chainAddress;
            if (restored) {
                a_original = nullptr;
                logger::info("{} rollback restored the original chain.", a_name);
            } else {
                logger::critical("{} rollback failed to restore the original chain.", a_name);
            }
            return restored;
        }
    }

    bool Install()
    {
        REL::Relocation<std::uintptr_t> playerControlsInputVtable{
            RE::VTABLE::PlayerControls__Manager[kPlayerControlsInputVtable]
        };
        const auto playerControlsHookAddress = REX::UNRESTRICTED_CAST<std::uintptr_t>(ProcessPlayerControlsInput);
        if (!InstallVtableHook(
                playerControlsInputVtable,
                kPerformInputProcessingSlot,
                playerControlsHookAddress,
                kExpectedPlayerControlsInputPrologue,
                originalPlayerControlsInput,
                "PlayerControls::Manager PerformInputProcessing hook")) {
            return false;
        }

        REL::Relocation<std::uintptr_t> uiInputVtable{
            RE::VTABLE::UI[kUIInputVtable]
        };
        const auto uiHookAddress = REX::UNRESTRICTED_CAST<std::uintptr_t>(ProcessUIInput);
        if (!InstallVtableHook(
                uiInputVtable,
                kPerformInputProcessingSlot,
                uiHookAddress,
                kExpectedUIInputPrologue,
                originalUIInput,
                "UI PerformInputProcessing hook")) {
            if (!RestoreVtableHook(
                    playerControlsInputVtable,
                    kPerformInputProcessingSlot,
                    playerControlsHookAddress,
                    originalPlayerControlsInput,
                    "PlayerControls::Manager PerformInputProcessing hook")) {
                logger::critical("PlayerControls input hook remained live after UI hook installation failed.");
            }
            return false;
        }

        return true;
    }
}
