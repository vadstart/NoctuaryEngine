-- Set the project name and version
set_project("NoctuaryEngine")
set_version("0.1")

-- Set the default language standard and build mode
set_languages("cxx20")
add_rules("mode.debug", "mode.release")

-- Add the main target
target("NoctuaryEngine")
set_kind("binary")
add_files("src/*.cpp")                              -- Add all your source files
add_includedirs("src", "$(env VULKAN_SDK)/include") -- Add include directories
add_linkdirs("$(env VULKAN_SDK)/lib")               -- Add Vulkan library directory



-- Platform-specific settings
if is_plat("linux") then
    add_links("glfw")
    add_links("vulkan")
    add_includedirs("usr/include")
    add_linkdirs("usr/lib")

    add_links("X11", "pthread", "dl", "m", "Xrandr", "Xi")
end

if is_plat("windows") then
    set_toolchains("msvc")
    set_runtimes("MD") -- dynamic CRT

    -- Ensure C++20 support with proper MSVC version
    set_languages("c++20")
    add_cxxflags("/std:c++20")

    -- Require minimum MSVC version for C++20
    add_cxxflags("/Zc:__cplusplus") -- Enable proper __cplusplus macro

    -- Fix Windows macro conflicts
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")

    -- Fix GLM GTX hash C++11 support issue
    add_defines("GLM_FORCE_CXX20", "GLM_ENABLE_EXPERIMENTAL")

    -- Suppress common Windows warnings
    add_cxxflags("/wd4005") -- macro redefinition
    add_cxxflags("/wd4996") -- deprecated function warnings
    add_cxxflags("/wd4068") -- unknown pragma warning

    -- Enable proper C++ conformance
    add_cxxflags("/permissive-")

    -- Clang + MinGW section
    -- set_toolchains("msvc")
    -- Use proper include paths for libc++ and headers
    -- add_cxxflags("-isystem", "C:/llvm-mingw/include/c++/v1", {force = true})
    -- add_cxxflags("-isystem", "C:/llvm-mingw/lib/clang/20/include", {force = true})
    -- add_cxxflags("-isystem", "C:/llvm-mingw/x86_64-w64-mingw32/include", {force = true})
    -- add_cxxflags("-D__USE_MINGW_ANSI_STDIO=1", {force = true})
    --
    -- add_ldflags("-L", "C:/llvm-mingw/x86_64-w64-mingw32/lib", {force = true})
    -- add_linkdirs("C:/VulkanSDK/Libraries/glfw/lib-mingw-w64")

    -- add_includedirs("src")
    add_includedirs("C:/VulkanSDK/Include")
    add_includedirs("C:/VulkanSDK/Libraries/glfw/include")
    add_includedirs("C:/VulkanSDK/Libraries/glm")

    add_linkdirs("C:/VulkanSDK/Lib")
    add_linkdirs("C:/VulkanSDK/Libraries/glfw/lib-vc2022")
    add_links("glfw3", "vulkan-1")

    add_syslinks("gdi32", "shell32", "user32", "opengl32")
end

if is_plat("macosx") then
    add_defines("VK_USE_PLATFORM_METAL_EXT") -- Use Metal for Vulkan on macOS
    add_links("glfw", "vulkan")
    add_includedirs("/opt/homebrew/include") -- Include GLFW headers
    add_linkdirs("/opt/homebrew/lib")        -- Link GLFW library
end

-- imGUI
add_files("src/imgui/*.cpp")
add_files("src/imgui/backends/*.cpp")
add_files("src/imgui/imgui.cpp")
add_files("src/imgui/imgui_draw.cpp")
add_files("src/imgui/imgui_demo.cpp")
add_files("src/imgui/imgui_tables.cpp")
add_files("src/imgui/imgui_widgets.cpp")
add_files("src/imgui/backends/imgui_impl_glfw.cpp")
add_files("src/imgui/backends/imgui_impl_vulkan.cpp")
-- Include directories
add_includedirs("src/imgui", "src/imgui/backends")

-- Add custom rules for shader compilation
after_build(function(target)
    import("core.project.project")

    local glslc = "$(env VULKAN_SDK)/bin/glslc"
    local shader_dir = "src/shaders"
    local output_dir = "shaders"

    os.mkdir(output_dir)

    for _, shader_file in ipairs(os.files(shader_dir .. "/*.vert")) do
        local filename = path.filename(shader_file)
        os.execv(glslc, { shader_file, "-o", path.join(output_dir, filename .. ".spv") })
    end

    for _, shader_file in ipairs(os.files(shader_dir .. "/*.frag")) do
        local filename = path.filename(shader_file)
        os.execv(glslc, { shader_file, "-o", path.join(output_dir, filename .. ".spv") })
    end

    -- Copy shaders to build directory
    os.cp("shaders/*.spv", path.join(target:targetdir(), "/shaders/"))
end)

on_clean(function(target)
    local shaderDir = target:targetdir() .. "/shaders/"
    os.rm(shaderDir) -- Remove the shaders directory
end)
