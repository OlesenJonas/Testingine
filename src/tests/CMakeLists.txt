set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/out/debug_tests)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/out/debug_tests)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/out/release_tests)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/out/release_tests)

add_executable(Span_test Span.cpp)
# link needed libraries
target_link_libraries(Span_test intern)
# Define the test
add_test(NAME "run_span_test" COMMAND $<TARGET_FILE:Span_test>)

add_executable(Pool_test Pool.cpp)
# link needed libraries
target_link_libraries(Pool_test intern)
# Define the test
add_test(NAME "run_pool_test" COMMAND $<TARGET_FILE:Pool_test>)

add_executable(Bitset_test Bitset.cpp)
# link needed libraries
target_link_libraries(Bitset_test intern)
# Define the test
add_test(NAME "run_bitset_test" COMMAND $<TARGET_FILE:Bitset_test>)

add_executable(ECS_test TestECS.cpp)
# link needed libraries
target_link_libraries(ECS_test intern)
target_link_libraries(ECS_test EASTL)
# Define the test
add_test(NAME "run_ECS_test" COMMAND $<TARGET_FILE:ECS_test>)