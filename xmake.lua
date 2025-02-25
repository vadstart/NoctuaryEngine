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
    add_links("vulkan", "glfw") -- Link Vulkan and GLFW
    add_linkdirs("$(env VULKAN_SDK)/lib") -- Add Vulkan library directory

    -- Platform-specific settings
    if is_plat("macosx") then
        add_defines("VK_USE_PLATFORM_METAL_EXT") -- Use Metal for Vulkan on macOS
        add_includedirs("/opt/homebrew/include") -- Include GLFW headers
        add_linkdirs("/opt/homebrew/lib") -- Link GLFW library
    end

    -- Add custom rules for shader compilation
    after_build(function (target)
        os.exec("$(env VULKAN_SDK)/bin/glslc src/shaders/simple_shader.vert -o shaders/simple_shader.vert.spv")
        os.exec("$(env VULKAN_SDK)/bin/glslc src/shaders/simple_shader.frag -o shaders/simple_shader.frag.spv")
    end)
