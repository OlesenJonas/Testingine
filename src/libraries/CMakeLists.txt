add_subdirectory(thirdparty)

add_subdirectory(intern)

target_compile_definitions(glm::glm INTERFACE "-DGLM_FORCE_DEPTH_ZERO_TO_ONE")
target_compile_definitions(glm::glm INTERFACE "-DGLM_FORCE_RADIANS")

# create list of all libraries that executables will link to
set(LIBS
    Vulkan::Vulkan glfw glm::glm VMA TinyOBJ stb intern
    PARENT_SCOPE)