#include "Input.h"
#include "DialogueCamera.h"
#include "REX/W32/XINPUT.h"

namespace ToggleDialogueCameraSF::Input
{
    namespace
    {
        constexpr std::int32_t kMouseWheelUp{ 0x800 };
        constexpr std::int32_t kMouseWheelDown{ 0x900 };

        // PlayerControls::Manager is the first normal receiver on Starfield 1.16.244.
        // Custom view inputs are consumed there before vanilla can claim them.
        constexpr std::size_t kPlayerControlsInputVtable = 8;
        constexpr std::size_t kThirdPersonStateVtable = 0;
        constexpr std::size_t kPerformInputProcessingSlot = 1;
        constexpr std::size_t kOnButtonEventSlot = 8;
        constexpr std::size_t kMaximumQueueLength = 512;
        constexpr std::array<std::uint8_t, 16> kExpectedPlayerControlsInputPrologue{
            0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x56, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x20
        };
        constexpr std::array<std::uint8_t, 16> kExpectedThirdPersonButtonPrologue{
            0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48
        };

        using PlayerControlsInputProcessor = void (*)(RE::BSInputEventReceiver*, const RE::InputEvent*);
        using ThirdPersonButtonProcessor = void (*)(RE::ThirdPersonState*, const RE::ButtonEvent*);
        PlayerControlsInputProcessor originalPlayerControlsInput{ nullptr };
        ThirdPersonButtonProcessor originalThirdPersonButtonInput{ nullptr };

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
            return MouseWheelDirection::kNone;
        }

        [[nodiscard]] bool TemporarilyExposeDisabledMouseWheel(
            RE::ButtonEvent& a_button,
            const MouseWheelDirection a_direction)
        {
            if (!DialogueCamera::ShouldRouteDisabledMouseWheelThroughThirdPerson() ||
                a_direction == MouseWheelDirection::kNone ||
                a_button.status != RE::InputEvent::Status::kUnhandled ||
                !a_button.disabled) {
                return false;
            }

            const std::string_view expectedEvent =
                a_direction == MouseWheelDirection::kIn ? "ZoomIn" : "ZoomOut";
            if (a_button.strUserEvent != expectedEvent) {
                logger::error(
                    "Disabled-mode mouse input retained its mask because the underlying event was {}, expected {}.",
                    a_button.strUserEvent.c_str(),
                    expectedEvent);
                return false;
            }

            a_button.disabled = false;
            logger::info(
                "Exposed disabled-mode mouse input for one native ThirdPersonState dispatch: "
                "direction={}, mapped event={}.",
                a_direction == MouseWheelDirection::kIn ? "in" : "out",
                a_button.strUserEvent.c_str());
            return true;
        }

        [[nodiscard]] bool IsViewButton(const RE::ButtonEvent& a_button)
        {
            // XInput's legacy BACK name is the Xbox View button (two overlapping rectangles).
            return a_button.deviceType == RE::InputEvent::DeviceType::kGamepad &&
                   a_button.idCode == REX::W32::XINPUT_GAMEPAD_BACK;
        }

        [[nodiscard]] bool ConsumeViewButton(
            const RE::ButtonEvent* a_button,
            const bool a_toggleAlreadyRequested)
        {
            if (!a_button || a_button->status == RE::InputEvent::Status::kStop || !IsViewButton(*a_button)) {
                return false;
            }

            auto* const mutableButton = const_cast<RE::ButtonEvent*>(a_button);
            const auto previousStatus = mutableButton->status;
            mutableButton->status = RE::InputEvent::Status::kStop;

            const bool initialPress = a_button->value != 0.0F && a_button->heldDownSecs == 0.0F;
            if (!initialPress || a_toggleAlreadyRequested) {
                return false;
            }

            logger::info(
                "View-button input accepted before PlayerControls: event={}, time={}, prior status={}.",
                a_button->QUserEvent().c_str(),
                a_button->timeCode,
                std::to_underlying(previousStatus));
            return true;
        }

        void ProcessPlayerControlsInput(RE::BSInputEventReceiver* a_receiver, const RE::InputEvent* a_queueHead)
        {
            RE::ButtonEvent* temporarilyExposedMouseWheelInput = nullptr;
            if (DialogueCamera::IsOpen()) {
                auto event = a_queueHead;
                std::size_t eventCount = 0;
                bool toggleRequested = false;
                const RE::ButtonEvent* firstMouseWheelInput = nullptr;
                while (event && eventCount < kMaximumQueueLength) {
                    if (event->eventType == RE::InputEvent::EventType::kButton) {
                        auto const button = static_cast<const RE::ButtonEvent*>(event);
                        if (ConsumeViewButton(button, toggleRequested)) {
                            toggleRequested = true;
                        }
                        if (!firstMouseWheelInput &&
                            button->status != RE::InputEvent::Status::kStop &&
                            button->value != 0.0F && button->heldDownSecs == 0.0F &&
                            GetMouseWheelDirection(*button) != MouseWheelDirection::kNone) {
                            firstMouseWheelInput = button;
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
                    DialogueCamera::Toggle();
                } else if (firstMouseWheelInput) {
                    const auto direction = GetMouseWheelDirection(*firstMouseWheelInput);
                    const bool zoomIn = direction == MouseWheelDirection::kIn;
                    auto* const mutableMouseWheelInput = const_cast<RE::ButtonEvent*>(firstMouseWheelInput);
                    if (DialogueCamera::HandleDisabledMouseWheel(zoomIn) ||
                        DialogueCamera::HandleMouseWheel(zoomIn)) {
                        const auto previousStatus = mutableMouseWheelInput->status;
                        mutableMouseWheelInput->status = RE::InputEvent::Status::kStop;

                        logger::info(
                            "Mouse dialogue-view input consumed before PlayerControls: "
                            "direction={}, id={}, event={}, time={}, prior status={}.",
                            direction == MouseWheelDirection::kIn ? "in" : "out",
                            firstMouseWheelInput->idCode,
                            firstMouseWheelInput->QUserEvent().c_str(),
                            firstMouseWheelInput->timeCode,
                            std::to_underlying(previousStatus));
                    } else if (TemporarilyExposeDisabledMouseWheel(*mutableMouseWheelInput, direction)) {
                        temporarilyExposedMouseWheelInput = mutableMouseWheelInput;
                    }
                }
            }

            originalPlayerControlsInput(a_receiver, a_queueHead);

            if (temporarilyExposedMouseWheelInput) {
                temporarilyExposedMouseWheelInput->disabled = true;
                logger::debug(
                    "Restored the disabled-mode mouse input mask after PlayerControls: mapped event={}, status={}.",
                    temporarilyExposedMouseWheelInput->strUserEvent.c_str(),
                    std::to_underlying(temporarilyExposedMouseWheelInput->status));
            }
        }

        void ProcessThirdPersonButtonInput(RE::ThirdPersonState* a_state, const RE::ButtonEvent* a_button)
        {
            MouseWheelDirection direction = MouseWheelDirection::kNone;
            bool stopAfterThirdPerson = false;
            if (a_state && a_button &&
                a_button->status != RE::InputEvent::Status::kStop &&
                a_button->value != 0.0F && a_button->heldDownSecs == 0.0F) {
                direction = GetMouseWheelDirection(*a_button);
                stopAfterThirdPerson =
                    direction != MouseWheelDirection::kNone &&
                    (DialogueCamera::ShouldStopDisabledMouseWheelAfterThirdPerson(*a_state) ||
                     DialogueCamera::ShouldStopMouseWheelAfterThirdPerson(
                         *a_state,
                         direction == MouseWheelDirection::kIn));
            }

            originalThirdPersonButtonInput(a_state, a_button);

            if (!stopAfterThirdPerson) {
                return;
            }

            auto* const mutableButton = const_cast<RE::ButtonEvent*>(a_button);
            const auto previousStatus = mutableButton->status;
            mutableButton->status = RE::InputEvent::Status::kStop;
            logger::info(
                "Mouse middle-distance input stopped after one ThirdPersonState dispatch: "
                "direction={}, id={}, event={}, time={}, prior status={}.",
                direction == MouseWheelDirection::kIn ? "in" : "out",
                a_button->idCode,
                a_button->QUserEvent().c_str(),
                a_button->timeCode,
                std::to_underlying(previousStatus));
        }

        template <class Function, std::size_t N>
        [[nodiscard]] bool InstallVtableHook(
            REL::Relocation<std::uintptr_t>& a_vtable,
            const std::size_t a_slot,
            const std::uintptr_t a_hookAddress,
            const std::array<std::uint8_t, N>& a_expectedPrologue,
            Function& a_original,
            const std::string_view a_name)
        {
            const auto slotAddress = a_vtable.address() + sizeof(std::uintptr_t) * a_slot;
            const auto originalAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
            if (!HasExpectedPrologue(originalAddress, a_expectedPrologue)) {
                logger::error("{} preflight failed; the input hook was not installed.", a_name);
                return false;
            }

            // Publish the chain target before making this hook reachable.
            a_original = REX::UNRESTRICTED_CAST<Function>(originalAddress);
            const auto replacedAddress = a_vtable.write_vfunc(a_slot, a_hookAddress);
            const auto chainAddress = replacedAddress != 0 ? replacedAddress : originalAddress;
            a_original = REX::UNRESTRICTED_CAST<Function>(chainAddress);

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

        template <class Function>
        [[nodiscard]] bool RestoreVtableHook(
            REL::Relocation<std::uintptr_t>& a_vtable,
            const std::size_t a_slot,
            const std::uintptr_t a_hookAddress,
            Function& a_original,
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

        REL::Relocation<std::uintptr_t> thirdPersonStateVtable{
            RE::VTABLE::ThirdPersonState[kThirdPersonStateVtable]
        };
        const auto thirdPersonButtonHookAddress =
            REX::UNRESTRICTED_CAST<std::uintptr_t>(ProcessThirdPersonButtonInput);
        if (!InstallVtableHook(
                thirdPersonStateVtable,
                kOnButtonEventSlot,
                thirdPersonButtonHookAddress,
                kExpectedThirdPersonButtonPrologue,
                originalThirdPersonButtonInput,
                "ThirdPersonState OnButtonEvent hook")) {
            if (!RestoreVtableHook(
                    playerControlsInputVtable,
                    kPerformInputProcessingSlot,
                    playerControlsHookAddress,
                    originalPlayerControlsInput,
                    "PlayerControls::Manager PerformInputProcessing hook")) {
                logger::critical(
                    "PlayerControls input hook remained live after ThirdPersonState hook installation failed.");
            }
            return false;
        }

        return true;
    }
}
