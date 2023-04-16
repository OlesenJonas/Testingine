cmake_minimum_required(VERSION 3.2)
include(DefaultLibrary)
target_link_libraries(SPIRV-Reflect PRIVATE Vulkan::Vulkan)