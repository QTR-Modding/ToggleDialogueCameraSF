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
        constexpr auto kMouseWheelQuietWindow = std::chrono::milliseconds{ 100 };

        // UI's BSInputEventReceiver subobject for Starfield 1.16.244.
        // Consuming here precedes DialogueMenu input dispatch and downstream camera handling.
        constexpr std::size_t kUIInputVtable = 10;
        constexpr std::size_t kPerformInputProcessingSlot = 1;
        constexpr std::size_t kMaximumQueueLength = 512;
        constexpr std::array<std::uint8_t, 16> kExpectedUIInputPrologue{
            0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20, 0x55
        };

        using InputProcessor = void (*)(RE::BSInputEventReceiver*, const RE::InputEvent*);
        InputProcessor originalUIInput{ nullptr };
        std::chrono::steady_clock::time_point lastMouseWheelInput;

        [[nodiscard]] bool HasExpectedPrologue(const std::uintptr_t a_address)
        {
            const auto text = REX::FModule::GetExecutingModule().GetSection(".text");
            const auto textAddress = text.GetAddress();
            const auto textSize = text.GetSize();
            if (a_address == 0 || textAddress == 0 || kExpectedUIInputPrologue.size() > textSize ||
                a_address < textAddress || a_address - textAddress > textSize - kExpectedUIInputPrologue.size()) {
                return false;
            }

            return std::memcmp(
                       reinterpret_cast<const void*>(a_address),
                       kExpectedUIInputPrologue.data(),
                       kExpectedUIInputPrologue.size()) == 0;
        }

        [[nodiscard]] bool IsMouseWheelInput(const RE::ButtonEvent& a_button)
        {
            if (a_button.deviceType != RE::InputEvent::DeviceType::kMouse) {
                return false;
            }

            const auto userEvent = std::string_view{ a_button.QUserEvent().c_str() };
            return userEvent == kZoomInEvent || userEvent == kZoomOutEvent ||
                   a_button.idCode == kMouseWheelUp || a_button.idCode == kMouseWheelDown;
        }

        [[nodiscard]] bool IsToggleInput(const RE::ButtonEvent& a_button)
        {
            const auto userEvent = std::string_view{ a_button.QUserEvent().c_str() };
            const auto& config = Settings::Get();
            if (a_button.deviceType == RE::InputEvent::DeviceType::kKeyboard) {
                return userEvent == kTogglePOVEvent ||
                       (config.keyboardToggleKey >= 0 && a_button.idCode == config.keyboardToggleKey);
            }
            if (a_button.deviceType == RE::InputEvent::DeviceType::kMouse) {
                return userEvent == kZoomInEvent || userEvent == kZoomOutEvent ||
                       a_button.idCode == kMouseWheelUp || a_button.idCode == kMouseWheelDown;
            }
            if (a_button.deviceType == RE::InputEvent::DeviceType::kGamepad) {
                return userEvent == kTogglePOVEvent ||
                       (config.gamepadToggleKey >= 0 && a_button.idCode == config.gamepadToggleKey);
            }
            return false;
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

            if (IsMouseWheelInput(*a_button)) {
                const auto now = std::chrono::steady_clock::now();
                const bool quietWindowElapsed = now - lastMouseWheelInput >= kMouseWheelQuietWindow;
                lastMouseWheelInput = now;
                if (!quietWindowElapsed) {
                    logger::debug(
                        "Mouse-wheel toggle pulse coalesced: id={}, event={}, time={}, value={}.",
                        a_button->idCode,
                        a_button->QUserEvent().c_str(),
                        a_button->timeCode,
                        a_button->value);
                    return false;
                }
            }

            if (a_toggleAlreadyRequested) {
                logger::debug(
                    "Additional toggle press in the same UI input queue was stopped: device={}, id={}, event={}.",
                    std::to_underlying(a_button->deviceType),
                    a_button->idCode,
                    a_button->QUserEvent().c_str());
                return false;
            }

            logger::info(
                "Toggle input accepted: device={}, id={}, event={}, time={}, prior status={}, value={}, held={}.",
                std::to_underlying(a_button->deviceType),
                a_button->idCode,
                a_button->QUserEvent().c_str(),
                a_button->timeCode,
                std::to_underlying(previousStatus),
                a_button->value,
                a_button->heldDownSecs);
            return true;
        }

        void ProcessUIInput(RE::BSInputEventReceiver* a_receiver, const RE::InputEvent* a_queueHead)
        {
            if (DialogueCamera::IsOpen()) {
                auto event = a_queueHead;
                std::size_t eventCount = 0;
                bool toggleRequested = false;
                while (event && eventCount < kMaximumQueueLength) {
                    if (event->eventType == RE::InputEvent::EventType::kButton) {
                        if (ConsumeToggleInput(
                                static_cast<const RE::ButtonEvent*>(event),
                                toggleRequested)) {
                            toggleRequested = true;
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

                if (toggleRequested) {
                    DialogueCamera::Toggle();
                }
            }

            originalUIInput(a_receiver, a_queueHead);
        }
    }

    bool Install()
    {
        REL::Relocation<std::uintptr_t> uiInputVtable{
            RE::VTABLE::UI[kUIInputVtable]
        };
        const auto slotAddress =
            uiInputVtable.address() + sizeof(std::uintptr_t) * kPerformInputProcessingSlot;
        const auto originalAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);

        if (!HasExpectedPrologue(originalAddress)) {
            logger::error("UI input receiver preflight failed; the input hook was not installed.");
            return false;
        }

        const auto hookAddress = REX::UNRESTRICTED_CAST<std::uintptr_t>(ProcessUIInput);
        originalUIInput = REX::UNRESTRICTED_CAST<InputProcessor>(originalAddress);
        const auto replacedAddress =
            uiInputVtable.write_vfunc(kPerformInputProcessingSlot, hookAddress);
        const auto chainAddress = replacedAddress != 0 ? replacedAddress : originalAddress;
        originalUIInput = REX::UNRESTRICTED_CAST<InputProcessor>(chainAddress);

        const bool originalMatched = replacedAddress == originalAddress;
        auto liveAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
        const bool hookMatched = liveAddress == hookAddress;
        if (!originalMatched || !hookMatched) {
            const bool rollbackAttempted = liveAddress == hookAddress;
            if (rollbackAttempted) {
                uiInputVtable.write_vfunc(kPerformInputProcessingSlot, chainAddress);
                liveAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
            }

            const bool rollbackVerified = !rollbackAttempted || liveAddress == chainAddress;
            const bool hookStillLive = liveAddress == hookAddress;
            if (!hookStillLive) {
                originalUIInput = nullptr;
            }
            logger::error(
                "UI input hook verification failed: original matched={}, hook readback={}, "
                "rollback attempted={}, rollback verified={}, hook still live={}.",
                originalMatched,
                hookMatched,
                rollbackAttempted,
                rollbackVerified,
                hookStillLive);
            return false;
        }

        logger::info("Installed UI PerformInputProcessing hook at {:X}.", originalAddress);
        return true;
    }
}
