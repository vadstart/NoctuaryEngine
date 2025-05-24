-- Set the project name and version
set_project("NoctuaryEngine")
set_version("0.1")

-- Set the default language standard and build mode
set_languages("cxx20")
add_rules("mode.debug", "mode.release")

-- Add the main target
target("NoctuaryEngine")
    set_kind("binary")
    add_files("src/*.cpp") -- Add all your source files
    add_includedirs("src", "$(env VULKAN_SDK)/include") -- Add include directories
    add_linkdirs("$(env VULKAN_SDK)/lib") -- Add Vulkan library directory

    

    -- Platform-specific settings
    if is_plat("linux") then
        add_links("glfw")
        add_links("vulkan")
        add_includedirs("usr/include")
        add_linkdirs("usr/lib")

        add_links("X11", "pthread", "dl", "m", "Xrandr", "Xi")
    end

    if is_plat("windows") then
        set_runtimes("MD") -- Use DLL runtime to match glfw3.lib build

        -- Include directories
        -- add_includedirs("src")
        add_includedirs("C:/VulkanSDK/Include")
        add_includedirs("C:/VulkanSDK/Libraries/glfw/include")
        add_includedirs("C:/VulkanSDK/Libraries/glm")

        -- Library directories
        add_linkdirs("C:/VulkanSDK/Lib") -- Vulkan SDK libraries
        add_linkdirs("C:/VulkanSDK/Libraries/glfw/lib-vc2022") -- GLFW built for MSVC

        -- Link libraries
        add_links("vulkan-1", "glfw3")
        add_syslinks("gdi32", "shell32", "user32", "opengl32")
    end

    if is_plat("macosx") then
        add_defines("VK_USE_PLATFORM_METAL_EXT") -- Use Metal for Vulkan on macOS
        add_links("glfw", "vulkan")      
        add_includedirs("/opt/homebrew/include") -- Include GLFW headers
        add_linkdirs("/opt/homebrew/lib") -- Link GLFW library
    end 

    -- imGUI
    -- add_files("src/imgui/*.cpp")
    -- add_files("src/imgui/backends/*.cpp")
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
    after_build(function (target)
        os.exec("$(env VULKAN_SDK)/bin/glslc src/shaders/simple_shader.vert -o shaders/simple_shader.vert.spv")
        os.exec("$(env VULKAN_SDK)/bin/glslc src/shaders/simple_shader.frag -o shaders/simple_shader.frag.spv")
        -- Copy shaders to build directory
        os.cp("shaders/*.spv", target:targetdir() .. "/shaders/")    
    end)

    on_clean(function (target)
        local shaderDir = target:targetdir() .. "/shaders/"
        os.rm(shaderDir)  -- Remove the shaders directory
    end)
