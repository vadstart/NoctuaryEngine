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
        set_runtimes("MD")
        add_cxxflags("/std:c++20")

        set_encodings("utf-8")
        set_encodings("source:utf-8", "target:utf-8")

        -- Vulkan paths
        local vk = "C:/VulkanSDK"
        add_includedirs(vk .. "/Include")
        add_linkdirs(vk .. "/Lib")
        add_links("vulkan-1")

        -- GLFW paths
        add_includedirs(vk .. "/Libraries/glfw/include")
        add_linkdirs(vk .. "/Libraries/glfw/lib-vc2022")
        add_links("glfw3")

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

    on_clean(function (target)
        local shaderDir = target:targetdir() .. "/shaders/"
        os.rm(shaderDir)  -- Remove the shaders directory
    end)
