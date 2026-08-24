#include "DialogueCamera.h"
#include "Settings.h"


namespace ToggleDialogueCameraSF::DialogueCamera
{
    namespace
    {
        constexpr std::string_view kDialogueMenuName{ "DialogueMenu" };
        constexpr std::string_view kTogglePOVEvent{ "TogglePOV" };
        constexpr std::int32_t kMouseWheelUp{ 8 };
        constexpr std::int32_t kMouseWheelDown{ 9 };

        // DialogueMenu's BSInputEventUser subobject for Starfield 1.16.244.
        constexpr std::size_t kDialogueMenuInputVtable = 1;
        constexpr std::size_t kOnButtonEventSlot = 8;
        constexpr std::array<std::uint8_t, 16> kExpectedButtonHandlerPrologue{
            0x48, 0x89, 0x5C, 0x24, 0x18, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57
        };

        using ButtonHandler = void (*)(RE::BSInputEventUser*, const RE::ButtonEvent*);

        std::atomic_bool dialogueOpen{ false };
        std::atomic_bool dialogueCameraEnabled{ false };
        std::atomic_bool controlsRestoreRequired{ false };
        std::atomic_bool installAttempted{ false };
        ButtonHandler originalButtonHandler{ nullptr };

        void ToggleCamera()
        {
            auto* const camera = RE::PlayerCamera::GetSingleton();
            if (!camera) {
                logger::warn("Toggle input received, but PlayerCamera is unavailable.");
                return;
            }

            const bool wasFirstPerson = camera->IsInFirstPerson();
            const bool wasThirdPerson = camera->IsInThirdPerson();
            const bool wasDialogueCamera = camera->QCameraEquals(RE::CameraState::kDialogue);

            const bool useDialogueCamera = dialogueCameraEnabled.load();
            std::string_view target;
            if (useDialogueCamera && wasDialogueCamera) {
                camera->ForceThirdPerson();
                controlsRestoreRequired.store(true);
                target = "third person";
            } else if (useDialogueCamera) {
                camera->SetCameraState(RE::CameraState::kDialogue);
                controlsRestoreRequired.store(false);
                target = "dialogue camera";
            } else if (wasFirstPerson) {
                camera->ForceThirdPerson();
                target = "third person";
            } else {
                camera->ForceFirstPerson();
                target = "first person";
            }

            logger::info(
                "Dialogue toggle received: first={}, third={}, dialogue={}, dialogue camera enabled={}, target={}.",
                wasFirstPerson,
                wasThirdPerson,
                wasDialogueCamera,
                useDialogueCamera,
                target);
        }

        [[nodiscard]] bool IsToggleInput(const RE::ButtonEvent& a_button)
        {
            if (a_button.QUserEvent() == kTogglePOVEvent) {
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

        void ProcessButton(RE::BSInputEventUser* a_receiver, const RE::ButtonEvent* a_button)
        {
            if (!dialogueOpen.load() || !a_button || !IsToggleInput(*a_button)) {
                originalButtonHandler(a_receiver, a_button);
                return;
            }

            if (a_button->status == RE::InputEvent::Status::kStop) {
                return;
            }

            if (a_button->value != 0.0F && a_button->heldDownSecs == 0.0F) {
                logger::info(
                    "TogglePOV input received: device={}, id={}, event={}.",
                    std::to_underlying(a_button->deviceType),
                    a_button->idCode,
                    a_button->QUserEvent().c_str());
                ToggleCamera();
            }
        }

        class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            static MenuSink* GetSingleton()
            {
                static MenuSink singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (a_event.menuName != kDialogueMenuName) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                dialogueOpen.store(a_event.opening);

                auto* const camera = RE::PlayerCamera::GetSingleton();
                if (!camera) {
                    dialogueCameraEnabled.store(false);
                    logger::warn("DialogueMenu {} but PlayerCamera is unavailable.", a_event.opening ? "opened" : "closed");
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (a_event.opening) {
                    controlsRestoreRequired.store(false);
                    const bool isFirstPerson = camera->IsInFirstPerson();
                    const bool isThirdPerson = camera->IsInThirdPerson();
                    const bool isDialogueCamera = camera->QCameraEquals(RE::CameraState::kDialogue);
                    dialogueCameraEnabled.store(isDialogueCamera);

                    logger::info(
                        "DialogueMenu opened: first={}, third={}, dialogue={}, dialogue camera enabled={}, auto={}.",
                        isFirstPerson,
                        isThirdPerson,
                        isDialogueCamera,
                        dialogueCameraEnabled.load(),
                        Settings::Get().autoToggle);

                    if (Settings::Get().autoToggle && !isFirstPerson) {
                        if (isDialogueCamera) {
                            controlsRestoreRequired.store(true);
                        }
                        camera->ForceFirstPerson();
                    }
                } else {
                    logger::info(
                        "DialogueMenu closed: dialogue camera enabled={}, auto={}.",
                        dialogueCameraEnabled.load(),
                        Settings::Get().autoToggle);

                    if (Settings::Get().autoToggle) {
                        camera->ForceThirdPerson();
                    }
                    if (controlsRestoreRequired.exchange(false)) {
                        if (auto* const player = RE::PlayerCharacter::GetSingleton()) {
                            player->SetControlsDriven(true);
                            logger::info("Restored player controls after leaving the dialogue camera state.");
                        } else {
                            logger::error("PlayerCharacter is unavailable; player controls could not be restored.");
                        }
                    }
                    dialogueCameraEnabled.store(false);
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    bool Install()
    {
        if (installAttempted.exchange(true)) {
            logger::error("Installation was requested more than once.");
            return false;
        }

        Settings::Load();

        auto* const ui = RE::UI::GetSingleton();
        if (!ui) {
            logger::error("UI singleton is unavailable.");
            return false;
        }

        REL::Relocation<std::uintptr_t> dialogueMenuInputVtable{ RE::VTABLE::DialogueMenu[kDialogueMenuInputVtable] };
        const auto slotAddress = dialogueMenuInputVtable.address() + sizeof(std::uintptr_t) * kOnButtonEventSlot;
        const auto originalAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
        if (originalAddress == 0 ||
            std::memcmp(reinterpret_cast<const void*>(originalAddress), kExpectedButtonHandlerPrologue.data(), kExpectedButtonHandlerPrologue.size()) != 0) {
            logger::error("DialogueMenu button handler preflight failed; hook was not installed.");
            return false;
        }

        originalButtonHandler = REX::UNRESTRICTED_CAST<ButtonHandler>(dialogueMenuInputVtable.write_vfunc(kOnButtonEventSlot, ProcessButton));
        const auto installedAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
        if (installedAddress != REX::UNRESTRICTED_CAST<std::uintptr_t>(ProcessButton)) {
            dialogueMenuInputVtable.write_vfunc(kOnButtonEventSlot, originalAddress);
            originalButtonHandler = nullptr;
            logger::error("DialogueMenu button handler readback failed; original target restored.");
            return false;
        }

        ui->RegisterSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
        logger::info("Installed DialogueMenu TogglePOV input hook at {:X}.", originalAddress);
        return true;
    }
}
