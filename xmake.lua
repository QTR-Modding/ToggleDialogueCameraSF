set_xmakever("3.0.9")
set_policy("package.requires_lock", true)

if is_plat("windows") then
    local project_root = os.projectdir()
    add_cxflags("/Brepro", "/experimental:deterministic", '/d1trimfile:"' .. project_root .. '"', '/pathmap:"' .. project_root .. '"=.', { tools = "cl", force = true })
    add_shflags("/Brepro", "/PDBALTPATH:%_PDB%", { force = true })
end

option("sf1sdk_dir")
    set_default("../SF1SDK")
    set_showmenu(true)
    set_description("Directory containing the SF1SDK checkout")
option_end()

includes(get_config("sf1sdk_dir") or "../SF1SDK")
add_requires("spdlog v1.16.0", { configs = { header_only = false, wchar = true, std_format = true } })

local plugin_name = "Toggle Dialogue Camera SF"
local dll_name = "ToggleDialogueCameraSF"
local plugin_version = "1.0.4"
local plugin_author = "Quantumyilmaz"
local major, minor, patch = plugin_version:match("^(%d+)%.(%d+)%.(%d+)$")
local packed = tonumber(major) * 16777216 + tonumber(minor) * 65536 + tonumber(patch) * 16

set_project(plugin_name)
set_version(plugin_version)
set_license("GPL-3.0-or-later")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg", "mode.release")
add_rules("plugin.vsxmake.autoupdate")

target(dll_name)
    set_kind("shared")
    add_rules("sf1sdk.plugin")
    add_deps("SF1SDK")
    add_packages("spdlog")
    add_files("src/Version.rc")
    add_defines(
        "NOMINMAX",
        'PLUGIN_NAME="' .. plugin_name .. '"',
        'PLUGIN_DLL_NAME="' .. dll_name .. '"',
        'PLUGIN_AUTHOR="' .. plugin_author .. '"',
        'PLUGIN_VERSION_TEXT="' .. plugin_version .. '.0"',
        "PLUGIN_VERSION_MAJOR=" .. major,
        "PLUGIN_VERSION_MINOR=" .. minor,
        "PLUGIN_VERSION_PATCH=" .. patch,
        "PLUGIN_PACKED_VERSION=" .. packed
    )
    set_pcxxheader("src/PCH.h")
    add_defines("_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING")
    add_files("src/plugin.cpp", "src/DialogueCamera.cpp", "src/Input.cpp", "src/DialogueMovementEnabler.cpp")
    add_headerfiles("src/PCH.h", "src/DialogueCamera.h", "src/Input.h", "src/DialogueMovementEnabler.h")
    add_includedirs("src")
