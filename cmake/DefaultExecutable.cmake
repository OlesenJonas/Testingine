get_filename_component(FOLDER_VAR ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project("${FOLDER_VAR}")

file(GLOB_RECURSE SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/*.c
	${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_compile_definitions(EXECUTABLE_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
add_compile_definitions(EXECUTABLE_NAME="${FOLDER_VAR}")

# Define the executable
add_executable(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} ${LIBS})

add_dependencies(${PROJECT_NAME} Shaders)