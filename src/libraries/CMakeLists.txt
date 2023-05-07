add_subdirectory(thirdparty)

add_subdirectory(intern)

target_compile_definitions(glm::glm INTERFACE "-DGLM_FORCE_DEPTH_ZERO_TO_ONE")
target_compile_definitions(glm::glm INTERFACE "-DGLM_FORCE_RADIANS")

# create list of all libraries that executables will link to
# todo: once executables interface correctly with the engine layer remove this
#       they really should only have to link against engine
set(LIBS
    Vulkan::Vulkan Vulkan::shaderc_combined SPIRV-Reflect glfw glm::glm VMA TinyOBJ stb ImGui intern
    PARENT_SCOPE)