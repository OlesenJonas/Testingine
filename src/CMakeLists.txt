include(vcpkgTargets)

# create libraries
add_subdirectory("libraries/")

# build shaders
# add_subdirectory("shaders/")
# Disabled since were loading shaders at runtime atm

# create executables
add_subdirectory("executables/")