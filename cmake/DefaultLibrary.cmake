get_filename_component(LIB ${CMAKE_CURRENT_SOURCE_DIR} NAME)

file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)
file(GLOB_RECURSE HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

set(LIBRARY_HAS_TESTS FALSE)
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Tests")
    set(LIBRARY_HAS_TESTS TRUE)
    list(FILTER SOURCES EXCLUDE REGEX ".*\\/Tests\\/.*")
    list(FILTER HEADERS EXCLUDE REGEX ".*\\/Tests\\/.*")
endif()

# main library
if(NOT SOURCES)
    add_library(${LIB} INTERFACE ${SOURCES})
    message(STATUS "Added Library: ${LIB} (Header only)")
    get_filename_component(DIR_ONE_ABOVE ../ ABSOLUTE)
    target_include_directories(${LIB} INTERFACE ${DIR_ONE_ABOVE})
else()
    add_library(${LIB} STATIC ${SOURCES})
    message(STATUS "Added Library: ${LIB}")
    target_include_directories(${LIB} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    get_filename_component(DIR_ONE_ABOVE ../ ABSOLUTE)
    target_include_directories(${LIB} PUBLIC ${DIR_ONE_ABOVE})
endif()

# tests
if(LIBRARY_HAS_TESTS)
    set(LIB_TESTS "${LIB}_TESTS_INTERFACE")

    add_library(${LIB_TESTS} INTERFACE)
    target_link_libraries(${LIB_TESTS} INTERFACE ${LIB})
    target_link_libraries(${LIB_TESTS} INTERFACE Testing)
    message(STATUS "Library has tests: ")

    file(GLOB Tests
        ${CMAKE_CURRENT_SOURCE_DIR}/Tests/*.cpp
    )
    FOREACH(test ${Tests})

        get_filename_component(TestName ${test} NAME_WE)
        set(TEST_EXECUTABLE "${LIB}Test${TestName}")

        add_executable(${TEST_EXECUTABLE} ${test})
        set_target_properties(${TEST_EXECUTABLE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/out/release_tests)
        set_target_properties(${TEST_EXECUTABLE} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/out/debug_tests)
        # link needed libraries
        target_link_libraries(${TEST_EXECUTABLE} PRIVATE ${LIB_TESTS})
        # Define the test
        add_test(NAME "run_${TEST_EXECUTABLE}" COMMAND $<TARGET_FILE:${TEST_EXECUTABLE}>)
        
        # message(STATUS ${test})
        message(STATUS "    ${TestName}")
        # message(STATUS ${TEST_EXECUTABLE})

    ENDFOREACH()
endif()
