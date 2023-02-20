include(FindVulkan)

# get libraries from vcpk
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# create libraries from local files
add_subdirectory("libraries/")

# build shaders
add_subdirectory("shaders/")

# create executables
add_subdirectory("executables/")

# create tests
add_subdirectory("tests/")