-- set minimum xmake version
set_xmakever("2.8.2")

-- include CommonLibSSE-NG (same template as dynamicGrip); adjust path if needed
includes("lib/CommonLibSSE-NG")

-- set project
set_project("SoulsStyleLooting")
set_version("0.0.0")
set_license("GPL-3.0")

-- set defaults
set_languages("c++23")
set_warnings("allextra")

-- add rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- build release with debug symbols so .pdb is generated (for crash debugging)
set_defaultmode("releasedbg")

-- require packages (optional; add as needed)
-- add_requires("simpleini")

-- targets
target("SoulsStyleLooting")
    add_deps("commonlibsse-ng")

    add_rules("commonlibsse-ng.plugin", {
        name = "SoulsStyleLooting",
        author = "libxse",
        description = "Souls-style looting for Skyrim"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
