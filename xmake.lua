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
        set_toolchains("msvc")
        set_runtimes("MD") -- dynamic CRT

        add_cxxflags("-std=c++20")

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
        add_linkdirs("/opt/homebrew/lib") -- Link GLFW library
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
    after_build(function (target)
        os.exec("$(env VULKAN_SDK)/bin/glslc src/shaders/simple_line_shader.vert -o shaders/simple_line_shader.vert.spv")
        os.exec("$(env VULKAN_SDK)/bin/glslc src/shaders/simple_shader.vert -o shaders/simple_shader.vert.spv")
        os.exec("$(env VULKAN_SDK)/bin/glslc src/shaders/simple_shader.frag -o shaders/simple_shader.frag.spv")
        -- Copy shaders to build directory
        os.cp("shaders/*.spv", target:targetdir() .. "/shaders/")    
    end)

    on_clean(function (target)
        local shaderDir = target:targetdir() .. "/shaders/"
        os.rm(shaderDir)  -- Remove the shaders directory
    end)
