add_subdirectory(thirdparty)

add_subdirectory(intern)

# create list of all libraries that executables will link to
set(LIBS Vulkan::Vulkan glfw glm::glm intern PARENT_SCOPE)