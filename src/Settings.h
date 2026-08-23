#pragma once

#include <cstdint>

namespace ToggleDialogueCameraSF::Settings
{
    struct Config
    {
        std::int32_t keyboardToggleKey{ 33 };
        std::int32_t gamepadToggleKey{ 128 };
        bool autoToggle{ false };
    };

    void Load();
    const Config& Get();
}
