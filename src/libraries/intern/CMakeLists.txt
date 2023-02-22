cmake_minimum_required(VERSION 3.2)
include(DefaultLibrary)
#need to link as public, so tests linking against "intern" also get them
target_link_libraries(intern PUBLIC Vulkan::Vulkan)
target_link_libraries(intern PUBLIC glfw)
target_link_libraries(intern PUBLIC glm::glm)