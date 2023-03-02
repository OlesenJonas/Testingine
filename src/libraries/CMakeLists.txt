add_subdirectory(thirdparty)

add_subdirectory(intern)

target_compile_definitions(glm::glm INTERFACE "-DGLM_FORCE_DEPTH_ZERO_TO_ONE")

# create list of all libraries that executables will link to
set(LIBS
    Vulkan::Vulkan glfw glm::glm VMA TinyOBJ intern
    PARENT_SCOPE)