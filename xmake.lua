set_xmakever("3.0.9")
set_policy("package.requires_lock", true)

if is_plat("windows") then
    local project_root = os.projectdir()
    add_cxflags("/Brepro", "/experimental:deterministic", '/d1trimfile:"' .. project_root .. '"', '/pathmap:"' .. project_root .. '"=.', { tools = "cl", force = true })
    add_shflags("/Brepro", "/PDBALTPATH:%_PDB%", { force = true })
end

includes(path.join(os.projectdir(), "lib", "commonlibsf"))

local plugin_name = "Toggle Dialogue Camera SF"
local dll_name = "ToggleDialogueCameraSF"
local plugin_version = "0.8.0"
local plugin_author = "Quantumyilmaz"

set_project(plugin_name)
set_version(plugin_version)
set_license("MIT")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg", "mode.release")
add_rules("plugin.vsxmake.autoupdate")

target(dll_name)
    add_rules("commonlibsf.plugin", {
        author = plugin_author,
        name = plugin_name,
        options = {
            sig_scanning = false,
            address_library = true,
            no_struct_use = false,
            layout_dependent = true
        }
    })
    set_pcxxheader("src/PCH.h")
    add_defines("_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING")
    add_files("src/plugin.cpp", "src/DialogueCamera.cpp", "src/Input.cpp", "src/Settings.cpp")
    add_headerfiles("src/PCH.h", "src/DialogueCamera.h", "src/Input.h", "src/Settings.h")
    add_includedirs("src")
