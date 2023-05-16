cmake_minimum_required(VERSION 3.2)

include(DefaultLibrary)

# dont like this being public, but for now basic vulkan enums are still used
# for selecting buffer/image usage/formats etc...
target_link_libraries(Engine PUBLIC Vulkan::Vulkan)
target_link_libraries(Engine PRIVATE Vulkan::shaderc_combined)
target_link_options(Engine INTERFACE "LINKER:/ignore:4099")
# if the above link option causes problems, just comment out the line if fixing it isnt trivial
# it just there because shaderc produces a ton of errors because glslang pdbs werent found
target_link_libraries(Engine PRIVATE SPIRV-Reflect)
target_link_libraries(Engine PRIVATE glfw)
target_link_libraries(Engine PRIVATE glm::glm)
# not sure why exactly this one needs to be public, but doesnt build otherwise
target_link_libraries(Engine PUBLIC VMA)
target_link_libraries(Engine PRIVATE ImGui)
target_link_libraries(Engine PRIVATE TinyOBJ)
target_link_libraries(Engine PRIVATE stb)
target_link_libraries(Engine PRIVATE EASTL)
target_link_libraries(Engine PRIVATE Datastructures)
target_link_libraries(Engine PRIVATE ECS)
target_link_libraries(Engine PRIVATE daw::daw-json-link)

IF(VULKAN_ENABLE_VALIDATION)
    message(STATUS "Compiling with vulkan validation enabled")
    target_compile_definitions(Engine PUBLIC ENABLE_VULKAN_VALIDATION)
ENDIF()