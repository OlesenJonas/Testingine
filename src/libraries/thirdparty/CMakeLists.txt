file(GLOB ITEMS ${CMAKE_CURRENT_SOURCE_DIR}/*)

FOREACH(item ${ITEMS})
    if(IS_DIRECTORY ${item})
        add_subdirectory(${item})
    ENDIF()
ENDFOREACH()

include(FetchContent)

FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.10
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(tracy)
