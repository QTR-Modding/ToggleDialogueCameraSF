#include "DialogueCamera.h"

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
        logger::critical("Unsupported Starfield runtime {}. This build requires {}.", runtime.string(), SFSE::RUNTIME_SF_1_16_244.string());
        return false;
    }

    logger::info("Toggle Dialogue Camera SF loading on Starfield {}.", runtime.string());
    return ToggleDialogueCameraSF::DialogueCamera::Install();
}
