#include "Settings.h"

#include <Windows.h>

namespace ToggleDialogueCameraSF::Settings
{
    namespace
    {
        constexpr auto kPath = L"Data\\SFSE\\Plugins\\ToggleDialogueCameraSF.ini";
        Config config;

        [[nodiscard]] std::int32_t ReadInt(const wchar_t* a_key, const std::int32_t a_default)
        {
            return static_cast<std::int32_t>(::GetPrivateProfileIntW(L"Settings", a_key, a_default, kPath));
        }
    }

    void Load()
    {
        config.keyboardToggleKey = ReadInt(L"iKeyboardToggleKey", config.keyboardToggleKey);
        config.gamepadToggleKey = ReadInt(L"iGamepadToggleKey", config.gamepadToggleKey);
        config.autoToggle = ReadInt(L"bAutoToggle", config.autoToggle ? 1 : 0) != 0;

        logger::info("Settings loaded: keyboard key {}, gamepad key {}, auto toggle {}.",
            config.keyboardToggleKey, config.gamepadToggleKey, config.autoToggle);
    }

    const Config& Get()
    {
        return config;
    }
}
