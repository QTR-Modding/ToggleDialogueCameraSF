#include "DialogueCamera.h"
#include "Settings.h"


namespace ToggleDialogueCameraSF::DialogueCamera
{
    namespace
    {
        constexpr std::string_view kDialogueMenuName{ "DialogueMenu" };

        // PlayerCamera's BSInputEventReceiver subobject for Starfield 1.16.244.
        constexpr std::size_t kInputReceiverVtable = 5;
        constexpr std::size_t kPerformInputProcessingSlot = 1;
        constexpr std::array<std::uint8_t, 13> kExpectedInputProcessorPrologue{
            0x48, 0x89, 0x5C, 0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x41
        };

        using InputProcessor = void (*)(RE::BSInputEventReceiver*, const RE::InputEvent*);

        std::atomic_bool dialogueOpen{ false };
        std::atomic_bool dialogueCameraEnabled{ false };
        std::atomic_bool installAttempted{ false };
        InputProcessor originalInputProcessor{ nullptr };

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
            if (wasDialogueCamera || wasFirstPerson) {
                camera->ForceThirdPerson();
                target = "third person";
            } else if (useDialogueCamera) {
                camera->SetCameraState(RE::CameraState::kDialogue);
                target = "dialogue camera";
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

        [[nodiscard]] bool IsToggleRelease(const RE::InputEvent& a_event)
        {
            if (a_event.eventType != RE::InputEvent::EventType::kButton) {
                return false;
            }

            const auto& button = static_cast<const RE::ButtonEvent&>(a_event);
            if (button.value != 0.0F || button.heldDownSecs == 0.0F) {
                return false;
            }

            const auto& config = Settings::Get();
            if (a_event.deviceType == RE::InputEvent::DeviceType::kKeyboard) {
                return config.keyboardToggleKey >= 0 && button.idCode == config.keyboardToggleKey;
            }
            if (a_event.deviceType == RE::InputEvent::DeviceType::kGamepad) {
                return config.gamepadToggleKey >= 0 && button.idCode == config.gamepadToggleKey;
            }
            return false;
        }

        void ProcessInput(RE::BSInputEventReceiver* a_receiver, const RE::InputEvent* a_events)
        {
            originalInputProcessor(a_receiver, a_events);
            if (!dialogueOpen.load() || !a_events) {
                return;
            }

            for (auto* event = a_events; event; event = event->next) {
                if (IsToggleRelease(*event)) {
                    ToggleCamera();
                    break;
                }
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

        REL::Relocation<std::uintptr_t> inputReceiverVtable{ RE::VTABLE::PlayerCamera[kInputReceiverVtable] };
        const auto slotAddress = inputReceiverVtable.address() + sizeof(std::uintptr_t) * kPerformInputProcessingSlot;
        const auto originalAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
        if (originalAddress == 0 ||
            std::memcmp(reinterpret_cast<const void*>(originalAddress), kExpectedInputProcessorPrologue.data(), kExpectedInputProcessorPrologue.size()) != 0) {
            logger::error("PlayerCamera input processor preflight failed; hook was not installed.");
            return false;
        }

        originalInputProcessor = REX::UNRESTRICTED_CAST<InputProcessor>(inputReceiverVtable.write_vfunc(kPerformInputProcessingSlot, ProcessInput));
        const auto installedAddress = *reinterpret_cast<const std::uintptr_t*>(slotAddress);
        if (installedAddress != REX::UNRESTRICTED_CAST<std::uintptr_t>(ProcessInput)) {
            inputReceiverVtable.write_vfunc(kPerformInputProcessingSlot, originalAddress);
            originalInputProcessor = nullptr;
            logger::error("PlayerCamera input processor readback failed; original target restored.");
            return false;
        }

        ui->RegisterSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
        logger::info("Installed PlayerCamera input hook at {:X}.", originalAddress);
        return true;
    }
}
