# include(FindVulkan)
find_package(Vulkan REQUIRED COMPONENTS dxc shaderc_combined)

# get libraries from vcpk
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(daw-json-link CONFIG REQUIRED)
find_package(EASTL CONFIG REQUIRED)
# need to set this again, seems like find_package(EASTL) overrides this...
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

target_compile_definitions(glm::glm INTERFACE "-DGLM_FORCE_DEPTH_ZERO_TO_ONE")
target_compile_definitions(glm::glm INTERFACE "-DGLM_FORCE_RADIANS")