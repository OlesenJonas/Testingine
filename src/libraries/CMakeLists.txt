add_subdirectory(thirdparty)

# create list of all libraries that executables will link to
set(LIBS Vulkan::Vulkan glfw glm::glm PARENT_SCOPE)