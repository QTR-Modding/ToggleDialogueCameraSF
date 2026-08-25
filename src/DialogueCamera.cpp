#include "DialogueCamera.h"
#include "Input.h"

namespace ToggleDialogueCameraSF::DialogueCamera
{
    namespace
    {
        constexpr std::string_view kDialogueMenuName{ "DialogueMenu" };
        constexpr std::string_view kMainMenuName{ "MainMenu" };
        constexpr std::string_view kPauseMenuName{ "PauseMenu" };
        constexpr std::string_view kDialogueCameraSettingName{ "bDialogueEnable:Interface" };

        enum class ResumeView : std::uint8_t
        {
            kNone,
            kFirstPerson,
            kThirdPerson
        };

        std::atomic_bool dialogueOpen{ false };
        std::atomic_bool pauseMenuOpen{ false };
        std::atomic_bool dialogueCameraEnabledAtOpen{ false };
        std::atomic_bool dialogueCameraOverrideActive{ false };
        std::atomic<ResumeView> resumeAfterSave{ ResumeView::kNone };
        std::atomic<ResumeView> resumeAfterPause{ ResumeView::kNone };
        std::atomic_bool installAttempted{ false };
        RE::Setting* dialogueCameraSetting{ nullptr };

        [[nodiscard]] const char* CameraStateName(const RE::PlayerCamera& a_camera)
        {
            if (a_camera.IsInFirstPerson()) {
                return "first person";
            }
            if (a_camera.IsInThirdPerson()) {
                auto const thirdPersonState = a_camera.GetThirdPersonState();
                if (!thirdPersonState) {
                    return "third person";
                }
                return thirdPersonState->IsCameraNearFarMode() ?
                           "far third person" :
                           "near third person";
            }
            if (a_camera.QCameraEquals(RE::CameraState::kDialogue)) {
                return "dialogue";
            }
            return "other";
        }

        [[nodiscard]] bool SetRuntimeDialogueCamera(const bool a_enabled)
        {
            if (!dialogueCameraSetting || !dialogueCameraSetting->Is<bool>()) {
                logger::error("The runtime Dialogue Camera setting is unavailable.");
                return false;
            }

            dialogueCameraSetting->SetBool(a_enabled);
            if (dialogueCameraSetting->GetBool() != a_enabled) {
                logger::error("Could not set the runtime Dialogue Camera gate to {}.", a_enabled);
                return false;
            }
            return true;
        }

        [[nodiscard]] bool RestoreDialogueCameraSetting(const std::string_view a_reason)
        {
            if (!dialogueCameraOverrideActive.load()) {
                return true;
            }

            const bool originalValue = dialogueCameraEnabledAtOpen.load();
            if (!SetRuntimeDialogueCamera(originalValue)) {
                logger::critical(
                    "Could not restore the runtime Dialogue Camera gate to {} during {}.",
                    originalValue,
                    a_reason);
                return false;
            }

            dialogueCameraOverrideActive.store(false);
            logger::info(
                "Restored the runtime Dialogue Camera gate to {} during {}.",
                originalValue,
                a_reason);
            return true;
        }

        [[nodiscard]] bool SuppressDialogueCamera(const std::string_view a_reason)
        {
            if (dialogueCameraOverrideActive.load()) {
                return true;
            }
            if (!SetRuntimeDialogueCamera(false)) {
                logger::error("Could not suppress the runtime Dialogue Camera gate during {}.", a_reason);
                return false;
            }

            dialogueCameraOverrideActive.store(true);
            logger::info("Suppressed the runtime Dialogue Camera gate during {}.", a_reason);
            return true;
        }

        [[nodiscard]] bool SelectThirdPerson(RE::PlayerCamera& a_camera, const bool a_fromDialogue)
        {
            if (a_fromDialogue) {
                // Generic state selection invokes Starfield's full dialogue-camera stop path.
                a_camera.SetCameraState(RE::CameraState::kThirdPerson);
            } else {
                a_camera.ForceThirdPerson();
            }
            return a_camera.IsInThirdPerson();
        }

        [[nodiscard]] bool SelectFirstPerson(RE::PlayerCamera& a_camera, const bool a_fromDialogue)
        {
            if (a_fromDialogue) {
                a_camera.SetCameraState(RE::CameraState::kFirstPerson);
            } else {
                a_camera.ForceFirstPerson();
            }
            return a_camera.IsInFirstPerson();
        }

        [[nodiscard]] bool ExitDialogueCameraToThirdPerson(
            RE::PlayerCamera& a_camera,
            const std::string_view a_reason)
        {
            if (!SuppressDialogueCamera(a_reason)) {
                return false;
            }
            if (SelectThirdPerson(a_camera, true)) {
                return true;
            }

            logger::error(
                "Dialogue-to-third-person transition failed during {}; camera remained {}.",
                a_reason,
                CameraStateName(a_camera));
            if (!RestoreDialogueCameraSetting("failed dialogue-to-third-person transition")) {
                logger::critical("Failed transition also left the runtime setting unrestored.");
            }
            return false;
        }

        [[nodiscard]] bool EnterDialogueCamera(
            RE::PlayerCamera& a_camera,
            const std::string_view a_reason)
        {
            auto const player = RE::PlayerCharacter::GetSingleton();
            if (!player) {
                logger::error("Dialogue-camera entry failed during {} because PlayerCharacter is unavailable.", a_reason);
                return false;
            }

            const ResumeView sourceView =
                a_camera.IsInFirstPerson() ? ResumeView::kFirstPerson :
                a_camera.IsInThirdPerson() ? ResumeView::kThirdPerson :
                                             ResumeView::kNone;
            if (sourceView == ResumeView::kNone) {
                logger::error(
                    "Dialogue-camera entry was rejected during {} because the source camera was {}.",
                    a_reason,
                    CameraStateName(a_camera));
                return false;
            }

            const bool ownedDialogueCameraOverride = dialogueCameraOverrideActive.load();
            if (!RestoreDialogueCameraSetting(a_reason)) {
                return false;
            }

            const bool started = player->StartDialogueCamera(false);
            const bool enteredDialogue = a_camera.QCameraEquals(RE::CameraState::kDialogue);
            if (!enteredDialogue) {
                logger::error(
                    "Dialogue-camera entry failed during {}: engine start returned {}, camera={}.",
                    a_reason,
                    started,
                    CameraStateName(a_camera));

                const bool suppressionRestored =
                    !ownedDialogueCameraOverride ||
                    SuppressDialogueCamera("failed dialogue-camera entry");
                bool failureViewRestored =
                    (sourceView == ResumeView::kFirstPerson && a_camera.IsInFirstPerson()) ||
                    (sourceView == ResumeView::kThirdPerson && a_camera.IsInThirdPerson());
                if (!failureViewRestored) {
                    if (sourceView == ResumeView::kFirstPerson) {
                        a_camera.SetCameraState(RE::CameraState::kFirstPerson);
                    } else {
                        a_camera.SetCameraState(RE::CameraState::kThirdPerson);
                    }

                    failureViewRestored =
                        (sourceView == ResumeView::kFirstPerson && a_camera.IsInFirstPerson()) ||
                        (sourceView == ResumeView::kThirdPerson && a_camera.IsInThirdPerson());
                }

                if (!suppressionRestored) {
                    logger::critical(
                        "Failed dialogue-camera entry could not reacquire the runtime Dialogue Camera override.");
                }
                if (!failureViewRestored) {
                    logger::critical(
                        "Failed dialogue-camera entry could not restore source view {}; camera remained {}.",
                        std::to_underlying(sourceView),
                        CameraStateName(a_camera));
                }
                return false;
            }

            if (!started) {
                logger::warn(
                    "Dialogue camera reached the requested state during {} although the engine start call returned false.",
                    a_reason);
            }
            return true;
        }

        [[nodiscard]] bool IsNonTerminalSave(const RE::SaveLoadEvent::OpType a_operation)
        {
            using OpType = RE::SaveLoadEvent::OpType;
            return a_operation == OpType::kAutosave ||
                   a_operation == OpType::kQuicksave ||
                   a_operation == OpType::kManualSave;
        }

        [[nodiscard]] ResumeView CaptureResumeView(const RE::PlayerCamera* a_camera)
        {
            if (!a_camera) {
                return ResumeView::kNone;
            }
            if (a_camera->IsInFirstPerson()) {
                return ResumeView::kFirstPerson;
            }
            if (a_camera->IsInThirdPerson()) {
                return ResumeView::kThirdPerson;
            }
            return ResumeView::kNone;
        }

        void ResumeDialogueView(const ResumeView a_view, const std::string_view a_reason)
        {
            if (a_view == ResumeView::kNone || !dialogueOpen.load()) {
                return;
            }

            auto const camera = RE::PlayerCamera::GetSingleton();
            if (!camera) {
                logger::error("Could not resume the dialogue view during {} because PlayerCamera is unavailable.", a_reason);
                return;
            }
            if (dialogueCameraEnabledAtOpen.load() && !SuppressDialogueCamera(a_reason)) {
                return;
            }

            const bool fromDialogue = camera->QCameraEquals(RE::CameraState::kDialogue);
            const bool selected =
                a_view == ResumeView::kFirstPerson ?
                    (camera->IsInFirstPerson() || SelectFirstPerson(*camera, fromDialogue)) :
                    (camera->IsInThirdPerson() || SelectThirdPerson(*camera, fromDialogue));
            if (!selected) {
                logger::error(
                    "Could not resume the dialogue view during {}; camera remained {}.",
                    a_reason,
                    CameraStateName(*camera));
                if (!RestoreDialogueCameraSetting("failed suspended-view restoration")) {
                    logger::critical("Suspended-view failure also left the runtime setting unrestored.");
                }
                return;
            }

            logger::info("Resumed the {} dialogue view during {}.", CameraStateName(*camera), a_reason);
        }

        class SaveLoadSink final : public RE::BSTEventSink<RE::SaveLoadEvent>
        {
        public:
            static SaveLoadSink* GetSingleton()
            {
                static SaveLoadSink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::SaveLoadEvent& a_event,
                RE::BSTEventSource<RE::SaveLoadEvent>*) override
            {
                using Status = RE::SaveLoadEvent::Status;

                if (a_event.status == Status::kBegin) {
                    const bool nonTerminalSave = IsNonTerminalSave(a_event.opType);
                    ResumeView suspendedView = ResumeView::kNone;
                    if (nonTerminalSave && dialogueOpen.load() && dialogueCameraOverrideActive.load()) {
                        suspendedView = CaptureResumeView(RE::PlayerCamera::GetSingleton());
                    }

                    if (!RestoreDialogueCameraSetting("save/load begin")) {
                        resumeAfterSave.store(ResumeView::kNone);
                        dialogueOpen.store(false);
                        return RE::BSEventNotifyControl::kContinue;
                    }

                    resumeAfterSave.store(suspendedView);
                    if (!nonTerminalSave) {
                        pauseMenuOpen.store(false);
                        resumeAfterPause.store(ResumeView::kNone);
                        dialogueOpen.store(false);
                        dialogueCameraEnabledAtOpen.store(false);
                    }

                    logger::info(
                        "Save/load begin reconciled: operation={}, suspended view={}.",
                        std::to_underlying(a_event.opType),
                        std::to_underlying(suspendedView));
                } else if (
                    a_event.status == Status::kSaveCompleted ||
                    a_event.status == Status::kFailed) {
                    const auto suspendedView = resumeAfterSave.exchange(ResumeView::kNone);
                    if (pauseMenuOpen.load() && suspendedView != ResumeView::kNone) {
                        if (resumeAfterPause.load() == ResumeView::kNone) {
                            resumeAfterPause.store(suspendedView);
                        }
                        logger::info("Deferred post-save dialogue-view restoration until PauseMenu closes.");
                    } else {
                        ResumeDialogueView(suspendedView, "save completion");
                    }
                }

                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            static MenuSink* GetSingleton()
            {
                static MenuSink singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent& a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                const auto menuName = std::string_view{ a_event.menuName.c_str() };
                if (menuName == kMainMenuName && a_event.opening) {
                    dialogueOpen.store(false);
                    pauseMenuOpen.store(false);
                    resumeAfterSave.store(ResumeView::kNone);
                    resumeAfterPause.store(ResumeView::kNone);
                    if (RestoreDialogueCameraSetting("MainMenu opening")) {
                        dialogueCameraEnabledAtOpen.store(false);
                        logger::info("MainMenu opening reconciled the dialogue-camera session.");
                    }
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (menuName == kPauseMenuName) {
                    if (a_event.opening) {
                        pauseMenuOpen.store(true);
                        resumeAfterPause.store(ResumeView::kNone);
                        if (dialogueOpen.load() && dialogueCameraOverrideActive.load()) {
                            const auto suspendedView = CaptureResumeView(RE::PlayerCamera::GetSingleton());
                            if (RestoreDialogueCameraSetting("PauseMenu opening")) {
                                resumeAfterPause.store(suspendedView);
                                logger::info(
                                    "PauseMenu opening suspended dialogue view {}.",
                                    std::to_underlying(suspendedView));
                            }
                        }
                    } else {
                        pauseMenuOpen.store(false);
                        auto suspendedView = resumeAfterPause.exchange(ResumeView::kNone);
                        if (dialogueOpen.load() && !dialogueCameraOverrideActive.load()) {
                            const bool liveSetting = dialogueCameraSetting->GetBool();
                            const bool capturedSetting = dialogueCameraEnabledAtOpen.load();
                            if (liveSetting != capturedSetting) {
                                dialogueCameraEnabledAtOpen.store(liveSetting);
                                logger::info(
                                    "PauseMenu close adopted the user-changed Dialogue Camera setting {}.",
                                    liveSetting);
                            }
                        }
                        ResumeDialogueView(suspendedView, "PauseMenu close");
                    }
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (menuName != kDialogueMenuName) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (a_event.opening && !RestoreDialogueCameraSetting("DialogueMenu opening")) {
                    dialogueOpen.store(false);
                    return RE::BSEventNotifyControl::kContinue;
                }

                auto const camera = RE::PlayerCamera::GetSingleton();
                if (!camera) {
                    dialogueOpen.store(false);
                    pauseMenuOpen.store(false);
                    resumeAfterSave.store(ResumeView::kNone);
                    resumeAfterPause.store(ResumeView::kNone);
                    if (!a_event.opening) {
                        if (RestoreDialogueCameraSetting("DialogueMenu close without PlayerCamera")) {
                            dialogueCameraEnabledAtOpen.store(false);
                        } else {
                            logger::critical("Dialogue camera state could not be reconciled without PlayerCamera.");
                        }
                    }
                    logger::error(
                        "DialogueMenu {} but PlayerCamera is unavailable.",
                        a_event.opening ? "opened" : "closed");
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (a_event.opening) {
                    pauseMenuOpen.store(false);
                    resumeAfterSave.store(ResumeView::kNone);
                    resumeAfterPause.store(ResumeView::kNone);
                    const bool settingEnabled = dialogueCameraSetting->GetBool();
                    dialogueCameraEnabledAtOpen.store(settingEnabled);
                    dialogueOpen.store(true);

                    logger::info(
                        "DialogueMenu opened: camera={}, dialogue camera setting={}.",
                        CameraStateName(*camera),
                        settingEnabled);
                } else {
                    dialogueOpen.store(false);
                    pauseMenuOpen.store(false);
                    resumeAfterSave.store(ResumeView::kNone);
                    resumeAfterPause.store(ResumeView::kNone);
                    logger::info(
                        "DialogueMenu closed: camera={}, dialogue camera setting={}, override={}.",
                        CameraStateName(*camera),
                        dialogueCameraSetting->GetBool(),
                        dialogueCameraOverrideActive.load());

                    if (!RestoreDialogueCameraSetting("DialogueMenu close")) {
                        return RE::BSEventNotifyControl::kContinue;
                    }
                    dialogueCameraEnabledAtOpen.store(false);

                    logger::info(
                        "Dialogue close reconciliation finished: camera={}, dialogue camera setting={}.",
                        CameraStateName(*camera),
                        dialogueCameraSetting->GetBool());
                }
                return RE::BSEventNotifyControl::kContinue;
            }
        };
    }

    bool IsOpen()
    {
        return dialogueOpen.load();
    }

    bool HandleMouseWheel(const bool a_zoomIn)
    {
        if (!dialogueOpen.load() || !dialogueCameraEnabledAtOpen.load()) {
            return false;
        }

        auto const camera = RE::PlayerCamera::GetSingleton();
        if (!camera) {
            logger::warn("Mouse-wheel input received, but PlayerCamera is unavailable.");
            return false;
        }

        if (a_zoomIn && camera->IsInFirstPerson() && dialogueCameraOverrideActive.load()) {
            if (EnterDialogueCamera(*camera, "mouse zoom-in boundary")) {
                logger::info("Mouse zoom-in boundary selected dialogue camera.");
            }
            return true;
        }
        if (!a_zoomIn && camera->QCameraEquals(RE::CameraState::kDialogue)) {
            if (ExitDialogueCameraToThirdPerson(*camera, "mouse zoom-out boundary")) {
                logger::info("Mouse zoom-out boundary selected third person.");
            }
            return true;
        }

        return false;
    }

    void Toggle()
    {
        if (!dialogueOpen.load()) {
            return;
        }

        auto const camera = RE::PlayerCamera::GetSingleton();
        if (!camera) {
            logger::warn("Toggle input received, but PlayerCamera is unavailable.");
            return;
        }

        const bool wasFirstPerson = camera->IsInFirstPerson();
        const bool wasThirdPerson = camera->IsInThirdPerson();
        const bool wasDialogueCamera = camera->QCameraEquals(RE::CameraState::kDialogue);
        const bool nativeDialogueCameraEnabled = dialogueCameraEnabledAtOpen.load();
        const char* target;

        if (nativeDialogueCameraEnabled) {
            if (wasDialogueCamera) {
                if (!ExitDialogueCameraToThirdPerson(*camera, "manual dialogue-to-third-person toggle")) {
                    return;
                }
                target = "near third person";
            } else if (wasThirdPerson) {
                auto const thirdPersonState = camera->GetThirdPersonState();
                if (!thirdPersonState) {
                    logger::error("Third-person cycle failed because ThirdPersonState is unavailable.");
                    return;
                }

                if (!thirdPersonState->IsCameraNearFarMode()) {
                    const bool overrideWasActive = dialogueCameraOverrideActive.load();
                    if (!SuppressDialogueCamera("manual near-third-person-to-far-third-person toggle")) {
                        return;
                    }

                    thirdPersonState->EnableCameraNearFarMode();
                    const bool farModeEnabled = thirdPersonState->IsCameraNearFarMode();
                    if (!camera->IsInThirdPerson() || !farModeEnabled) {
                        logger::error(
                            "Near-to-far third-person transition failed: camera={}, CameraNearFar mode={}.",
                            CameraStateName(*camera),
                            farModeEnabled);
                        if (!overrideWasActive &&
                            !RestoreDialogueCameraSetting("failed near-to-far third-person toggle")) {
                            logger::critical(
                                "Failed far-camera transition also left the runtime setting unrestored.");
                        }
                        return;
                    }
                    target = "far third person";
                } else {
                    if (!EnterDialogueCamera(*camera, "manual far-third-person-to-dialogue toggle")) {
                        return;
                    }
                    target = "dialogue camera";
                }
            } else {
                if (!SuppressDialogueCamera("manual first/other-to-third-person toggle")) {
                    return;
                }
                if (!SelectThirdPerson(*camera, false)) {
                    logger::error("First/other-to-third-person toggle failed; camera remained {}.", CameraStateName(*camera));
                    if (!RestoreDialogueCameraSetting("failed first/other-to-third-person toggle")) {
                        logger::critical("Failed transition also left the runtime setting unrestored.");
                    }
                    return;
                }
                target = "third person";
            }
        } else if (wasFirstPerson) {
            if (!SelectThirdPerson(*camera, false)) {
                logger::error("First-to-third-person toggle failed; camera remained {}.", CameraStateName(*camera));
                return;
            }
            target = "third person";
        } else if (wasThirdPerson) {
            if (!SelectFirstPerson(*camera, false)) {
                logger::error("Third-to-first-person toggle failed; camera remained {}.", CameraStateName(*camera));
                return;
            }
            target = "first person";
        } else if (wasDialogueCamera) {
            if (!SelectThirdPerson(*camera, true)) {
                logger::error("Unexpected dialogue-to-third-person toggle failed; camera remained {}.", CameraStateName(*camera));
                return;
            }
            target = "third person after unexpected dialogue state";
        } else {
            if (!SelectFirstPerson(*camera, false)) {
                logger::error("Other-to-first-person toggle failed; camera remained {}.", CameraStateName(*camera));
                return;
            }
            target = "first person";
        }

        logger::info(
            "Dialogue toggle: before[first={}, third={}, dialogue={}], native setting={}, override={}, target={}, after={}.",
            wasFirstPerson,
            wasThirdPerson,
            wasDialogueCamera,
            nativeDialogueCameraEnabled,
            dialogueCameraOverrideActive.load(),
            target,
            CameraStateName(*camera));
    }

    bool Install()
    {
        if (installAttempted.exchange(true)) {
            logger::warn("Dialogue camera installation was already attempted.");
            return false;
        }

        auto const ui = RE::UI::GetSingleton();
        if (!ui) {
            logger::error("UI singleton is unavailable.");
            return false;
        }

        auto const saveLoadSource = RE::SaveLoadEvent::GetEventSource();
        if (!saveLoadSource) {
            logger::error("SaveLoadEvent source is unavailable.");
            return false;
        }

        dialogueCameraSetting = RE::GetINISetting(kDialogueCameraSettingName);
        if (!dialogueCameraSetting || !dialogueCameraSetting->Is<bool>()) {
            logger::error("Could not resolve {} as a Boolean INI preference.", kDialogueCameraSettingName);
            dialogueCameraSetting = nullptr;
            return false;
        }

        if (!Input::Install()) {
            logger::error("The dialogue input hook could not be installed.");
            return false;
        }

        ui->RegisterSink<RE::MenuOpenCloseEvent>(MenuSink::GetSingleton());
        saveLoadSource->RegisterSink(SaveLoadSink::GetSingleton());
        logger::info(
            "Dialogue camera support installed; {} currently {}.",
            kDialogueCameraSettingName,
            dialogueCameraSetting->GetBool());
        return true;
    }
}
