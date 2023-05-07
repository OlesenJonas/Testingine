# include(FindVulkan)
find_package(Vulkan REQUIRED COMPONENTS shaderc_combined)

# get libraries from vcpk
find_package(EASTL CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# need to set this again, seems like find_package(EASTL) overrides this...
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# create libraries from local files
add_subdirectory("libraries/")

# build shaders
# add_subdirectory("shaders/")
# Disabled since were loading shaders at runtime atm

# create executables
add_subdirectory("executables/")

# create tests
add_subdirectory("tests/")