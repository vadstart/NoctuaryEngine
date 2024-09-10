cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(NoctuaryEngine VERSION 0.1)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Paths to GLFW and GLM
# Platform specific
if (WIN32)
  set(GLFW_INCLUDE_DIR "C:/Program Files/Microsoft Visual Studio/2022/Community/Libraries/glfw-3.4.bin.WIN64/include")
  set(GLFW_LIBRARY_DIR "C:/Program Files/Microsoft Visual Studio/2022/Community/Libraries/glfw-3.4.bin.WIN64/lib-vc2022")
  set(GLFW_LIB "C:/Program Files/Microsoft Visual Studio/2022/Community/Libraries/glfw-3.4.bin.WIN64/lib-vc2022/glfw3.lib")

  set(GLM_INCLUDE_DIR "C:/Program Files/Microsoft Visual Studio/2022/Community/Libraries/glm-1.0.1")
elseif (APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GLFW REQUIRED glfw3)
  include_directories(${GLFW_INCLUDE_DIRS})
  link_directories(${GLFW_LIBRARY_DIRS})

  find_package(GLM REQUIRED)
  include_directories(${GLM_INCLUDE_DIRS})

else()
  message(FATAL_ERROR "Unsupported platform")
endif()

# Find Vulkan SDK
find_package(Vulkan REQUIRED)

# Include directories
include_directories(${Vulkan_INCLUDE_DIRS})
include_directories(${GLFW_INCLUDE_DIR})
include_directories(${GLM_INCLUDE_DIR})

# Link libraries
link_directories(${GLFW_LIBRARY_DIR})
link_libraries(${Vulkan_LIBRARIES})
link_libraries(${GLFW_LIB})

# Gather all source files
file(GLOB SOURCES "src/*.cpp" "src/*.h")
message(STATUS "Source files: ${SOURCES}")

# Add the executable
add_executable(NoctuaryEngine ${SOURCES})

# Link the libraries to the executable
target_link_libraries(NoctuaryEngine ${Vulkan_LIBRARIES} ${GLFW_LIB} ${GLFW_LIBRARIES})

# Shader Compilation Step
set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/src/shaders")
set(SHADER_BINARY_DIR "${CMAKE_BINARY_DIR}/shaders")

# List of shaders to compile
set(SHADER_FILES
    simple_shader.vert
    simple_shader.frag
)

# Ensure the shader output directory exists
file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

foreach(SHADER ${SHADER_FILES})
    set(SHADER_SOURCE ${SHADER_SRC_DIR}/${SHADER})
    set(SHADER_OUTPUT ${SHADER_BINARY_DIR}/${SHADER}.spv)

    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${SHADER_SOURCE} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER_SOURCE}
        COMMENT "Compiling shader: ${SHADER}"
    )

    list(APPEND SHADER_OUTPUTS ${SHADER_OUTPUT})
endforeach()

# Add the shaders as dependencies to the build
add_custom_target(Shaders DEPENDS ${SHADER_OUTPUTS})

# Ensure shaders are built before the main executable
add_dependencies(NoctuaryEngine Shaders)

# Enable compile_commands.json generation
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
