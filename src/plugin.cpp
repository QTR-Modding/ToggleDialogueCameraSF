#include "DialogueCamera.h"

namespace
{
    void OnMessage(SFSE::MessagingInterface::Message* a_message)
    {
        if (!a_message || a_message->type != SFSE::MessagingInterface::kPostDataLoad) {
            return;
        }

        logger::info("SFSE post-data-load received; installing dialogue camera support.");
        if (!ToggleDialogueCameraSF::DialogueCamera::Install()) {
            logger::critical("Dialogue camera installation failed.");
        }
    }
}

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* a_sfse)
{
    if (!a_sfse) {
        return false;
    }

    constexpr SFSE::InitInfo initInfo{
        .logPattern = "%Y-%m-%d %H:%M:%S.%e [%l] %v"
    };
    SFSE::Init(a_sfse, initInfo);

    const auto runtime = a_sfse->RuntimeVersion();
    if (runtime != SFSE::RUNTIME_SF_1_16_244) {
        logger::critical(
            "Unsupported Starfield runtime {}. This build requires {}.",
            runtime.string(),
            SFSE::RUNTIME_SF_1_16_244.string());
        return false;
    }

    const auto* const messaging = SFSE::GetMessagingInterface();
    if (!messaging) {
        logger::critical("Required SFSE MessagingInterface is unavailable.");
        return false;
    }
    if (!messaging->RegisterListener(OnMessage)) {
        logger::critical("Could not register the SFSE lifecycle listener.");
        return false;
    }

    logger::info(
        "Toggle Dialogue Camera SF initialized on Starfield {}; waiting for post-data-load.",
        runtime.string());
    return true;
}
