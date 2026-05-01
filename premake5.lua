local function get_cmake_vs_generator()
    if _ACTION == "vs2026" then
        return "Visual Studio 18 2026"
    elseif _ACTION == "vs2022" then
        return "Visual Studio 17 2022"
    elseif _ACTION == "vs2019" then
        return "Visual Studio 16 2019"
    elseif _ACTION == "vs2017" then
        return "Visual Studio 15 2017"
    end

    error("This premake file currently supports vs2017/vs2019/vs2022/vs2026 only.")
end

local CMAKE_GENERATOR = get_cmake_vs_generator()

workspace "Bilibili live client"
    filename "BliveClient"
    configurations { "Debug", "Release" }
    startproject "BliveClient"
    local outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    defines { "IMGUI_INI_PATH=\"".."bin/" .. outputdir .."/%{prj.name}/ImGui.ini\"" }
    defines { "ROOT_PATH=\"".."bin/" .. outputdir .."/%{prj.name}\"" }

    filter "system:windows"
        defines { "__WINDOWS__" }
        architecture "x86_64"
        cppdialect "C++latest"
        systemversion "latest"
        buildoptions {"/utf-8","/bigobj"}
        includedirs { "$(SolutionDir)/External/vcpkg_installed/x64-windows/include"}
        libdirs { "$(SolutionDir)/External/vcpkg_installed/x64-windows/lib" }

        filter "action:vs*"
            vsprops {
                VcpkgEnableManifest = "true",
                VcpkgTriplet = "x64-windows",
                VcpkgInstalledDir    = "$(SolutionDir)/External/vcpkg_installed"
            }

    filter "configurations:Debug"
        symbols "On"
        runtime "Debug"
        vsprops { VcpkgConfiguration = "Debug" }

    filter "configurations:Release"
        optimize "On"
        runtime "Release"
        vsprops { VcpkgConfiguration = "Release" }

    filter {}

project "BliveClient"
    kind "ConsoleApp"
    language "C++"
    targetdir ("bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}") 

    includedirs  {"./Src"}
    includedirs  {"./External/Spdlog/include"}
    includedirs  {"./External/directx"}
    includedirs  {"./External/ImGui"}
    includedirs  {"./External/ImGui/backends"}
    includedirs  {"./External/ImGui/misc/cpp"}

    pchheader "stdafx.h"
    pchsource "Src/stdafx.cpp"

    postbuildcommands {
        "{COPYDIR} Resources %[bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/%{prj.name}/Resources]"
    }

    files {
        "Src/**.h",
        "Src/**.cpp",
        "Src/**.hpp",
        "Src/**.c",
        "External/Spdlog/include/**.h",
        "External/Spdlog/include/**.hpp",
        "External/directx/**.h",
        "External/ImGui/*.h",
        "External/ImGui/*.cpp",
        "External/ImGui/backends/imgui_impl_dx12.h",
        "External/ImGui/backends/imgui_impl_dx12.cpp",
        "External/ImGui/backends/imgui_impl_win32.h",
        "External/ImGui/backends/imgui_impl_win32.cpp",
        "External/ImGui/misc/cpp/*.h",
        "External/ImGui/misc/cpp/*.cpp",
    }

    filter { "files:External/ImGui/*.cpp"}
        enablepch "Off"

    filter { "files:External/ImGui/backends/*.cpp"}
        enablepch "Off"

    filter { "files:External/ImGui/misc/cpp/*.cpp"}
        enablepch "Off"

    filter {}

    links {
        "user32.lib",
    }

    filter "configurations:Debug"
        defines { "_DEBUG", "DEBUG" }
        links {
            "dxguid.lib",
        }

    filter "configurations:Release"
        defines { "NDEBUG" }

    
