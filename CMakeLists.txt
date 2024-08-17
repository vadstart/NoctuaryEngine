cmake_minimum_required(VERSION 3.10)

# Set the project name and version
project(NoctuaryEngine VERSION 0.1)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Paths to GLFW and GLM
set(GLFW_INCLUDE_DIR "C:/Program Files/Microsoft Visual Studio/2022/Community/Libraries/glfw-3.4.bin.WIN64/include")
set(GLFW_LIBRARY_DIR "C:/Program Files/Microsoft Visual Studio/2022/Community/Libraries/glfw-3.4.bin.WIN64/lib-vc2022")
set(GLFW_LIB "C:/Program Files/Microsoft Visual Studio/2022/Community/Libraries/glfw-3.4.bin.WIN64/lib-vc2022/glfw3.lib")

set(GLM_INCLUDE_DIR "C:/Program Files/Microsoft Visual Studio/2022/Community/Libraries/glm-1.0.1")

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
target_link_libraries(NoctuaryEngine ${Vulkan_LIBRARIES} ${GLFW_LIB})

