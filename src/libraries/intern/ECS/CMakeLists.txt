cmake_minimum_required(VERSION 3.2)

add_library(ECS STATIC ECS.cpp)
target_include_directories(ECS PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(DIR_ONE_ABOVE ../ ABSOLUTE)
target_include_directories(ECS PUBLIC ${DIR_ONE_ABOVE})

target_link_libraries("ECS" PRIVATE EASTL)

message(STATUS "Added Library: ECS")

# ----

add_executable(ECS_test Tests.cpp)
set_target_properties(ECS_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/out/release_tests)
set_target_properties(ECS_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/out/debug_tests)
# link needed libraries
target_link_libraries(ECS_test PRIVATE EASTL)
target_link_libraries(ECS_test PRIVATE ECS)
# Define the test
add_test(NAME "run_ECS_test" COMMAND $<TARGET_FILE:ECS_test>)