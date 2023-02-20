set(SHADER_OUTPUT_LOCATION ${CMAKE_BINARY_DIR}/out/shaders)

#Copy shader folder structure so destination paths for glslc exist!
file(GLOB_RECURSE folders LIST_DIRECTORIES true RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *)
FOREACH(item ${folders})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${item})
        file(MAKE_DIRECTORY ${SHADER_OUTPUT_LOCATION}/${item})
    ENDIF()
ENDFOREACH()


file(GLOB_RECURSE shaderFiles RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} * )
list(FILTER shaderFiles EXCLUDE REGEX "CMakeLists\\.txt")
#Exclude .glsl files, those are only used as includes in other shader sources
list(FILTER shaderFiles EXCLUDE REGEX ".*\\.glsl")

add_custom_target(Shaders)

FOREACH(shaderFile ${shaderFiles})
    
    # message(STATUS "Shader file found:")
    # message(STATUS ${shaderFile})
    set(shaderFileTargetName shaderFile_${shaderFile})
    STRING(REGEX REPLACE "(/|\\|\.)" "_" shaderFileTargetName ${shaderFileTargetName})
    # message(STATUS ${shaderFileTargetName})

    set(SHADERFILE_OUT "${SHADER_OUTPUT_LOCATION}/${shaderFile}.spirv")

    add_custom_command(
        OUTPUT ${SHADERFILE_OUT}
        COMMAND glslc "${CMAKE_CURRENT_SOURCE_DIR}/${shaderFile}"
                   -o ${SHADERFILE_OUT}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shaderFile}
        VERBATIM)
    add_custom_target(${shaderFileTargetName} DEPENDS ${SHADERFILE_OUT})
    add_dependencies(Shaders ${shaderFileTargetName})
ENDFOREACH()
