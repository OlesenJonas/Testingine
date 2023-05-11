cmake_minimum_required(VERSION 3.2)

include(DefaultLibrary)

# dont like this being public, but for now basic vulkan enums are still used
# for selecting buffer/image usage/formats etc...
target_link_libraries(intern PUBLIC Vulkan::Vulkan)
target_link_libraries(intern PRIVATE Vulkan::shaderc_combined)
target_link_libraries(intern PRIVATE SPIRV-Reflect)
target_link_libraries(intern PRIVATE glfw)
target_link_libraries(intern PRIVATE glm::glm)
# not sure why exactly this one needs to be public, but doesnt build otherwise
target_link_libraries(intern PUBLIC VMA)
target_link_libraries(intern PRIVATE ImGui)
target_link_libraries(intern PRIVATE TinyOBJ)
target_link_libraries(intern PRIVATE stb)
target_link_libraries(intern PRIVATE EASTL)

IF(VULKAN_ENABLE_VALIDATION)
    message(STATUS "Compiling with vulkan validation enabled")
    target_compile_definitions(intern PUBLIC ENABLE_VULKAN_VALIDATION)
ENDIF()