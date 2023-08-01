cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

##############################################################################

set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
# alternatively set in settings.json instead (vscode):
# "cmake.configureSettings": {
# 	"CMAKE_TOOLCHAIN_FILE": "${env:VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
# }

set(VCPKG_MANIFEST_MODE ON)
set(VCPKG_MANIFEST_DIR  ${CMAKE_CURRENT_SOURCE_DIR})

##############################################################################

project(Testingine)

##############################################################################

#Compiler Commands etc

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# clangd requires a compile_commands.json file
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if("${CMAKE_GENERATOR}" MATCHES "(Win64|IA64)")
	SET(BUILD_TYPE_64_BIT true)
endif()

if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
   SET(BUILD_TYPE_64_BIT true)
endif()

if(CMAKE_CL_64)
   SET(BUILD_TYPE_64_BIT true)
endif()

IF(BUILD_TYPE_64_BIT)
	  set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS TRUE)
ENDIF()

add_definitions(-DNOMINMAX) #dont want windows.h or other predefined min/max functions
# add_definitions(-D_CRT_SECURE_NO_WARNINGS)

#Set desired compile commands here. not forcing any atm, using clang tidy for warnings in editor
#Add as desired
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		message("Using Clang-cl MSVC Compiler")
		add_compile_options(/W4)
		#add_compile_options(/W4 /WX)
	else()
		message("Using Clang Compiler")
		# add_compile_options(-Wall)
		# add_compile_options(-Wall -Wextra -pedantic)
	endif()
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	message("Using GNU Compiler")
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
	message("Using Intel Compiler")
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	message("Using MSCV Compiler")
    # add_compile_options(/W4 /WX)
endif()

# "TODO: Decide on paths for release and copy files there post build
add_compile_definitions(
	$<$<CONFIG:DEBUG>:ASSETS_PATH="${CMAKE_SOURCE_DIR}/assets">
	$<$<CONFIG:RELEASE>:ASSETS_PATH="${CMAKE_SOURCE_DIR}/assets">
	$<$<CONFIG:RELWITHDEBINFO>:ASSETS_PATH="${CMAKE_SOURCE_DIR}/assets">
)

add_compile_definitions(
	$<$<CONFIG:DEBUG>:SHADERS_PATH="${CMAKE_SOURCE_DIR}/src/shaders">
	$<$<CONFIG:RELEASE>:SHADERS_PATH="${CMAKE_SOURCE_DIR}/src/shaders">
	$<$<CONFIG:RELWITHDEBINFO>:SHADERS_PATH="${CMAKE_SOURCE_DIR}/src/shaders">
)

##############################################################################

#enable testing
include(CTest)
list(APPEND CMAKE_CTEST_ARGUMENTS "--verbose")

##############################################################################

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/debug)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/lib/debug)
#on windows RUNTIME_OUTPUT decides where .dll s will be put
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/out/debug)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib/release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/lib/release)
#on windows RUNTIME_OUTPUT decides where .dll s will be put
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/out/release)

##############################################################################

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

add_subdirectory("${CMAKE_SOURCE_DIR}/src/")