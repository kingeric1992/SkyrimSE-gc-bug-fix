cmake_minimum_required(VERSION 3.26)


if(DEFINED CACHE{BUILD_NUMBER})
math(EXPR BUILD_NUMBER "${BUILD_NUMBER}+1")
message("BUILD_NUMBER defined." "${BUILD_NUMBER}")
else()
set(BUILD_NUMBER, "1")
message("BUILD_NUMBER not defined." "${BUILD_NUMBER}")
endif()

set(BUILD_NUMBER "${BUILD_NUMBER}" CACHE STRING "build number" FORCE )

project(
    GCBugFix
    VERSION 1.1.0.${BUILD_NUMBER}
    LANGUAGES CXX
    DESCRIPTION "An SkyrimSE/AE SKSE\" DEBUG_STR \" plugin to patch Array/Object GC bug."
    HOMEPAGE_URL "https://github.com/kingeric1992/SkyrimSE-gc-bug-fix"
)

set(PROJECT_RESOURCES_PATH "${CMAKE_CURRENT_LIST_DIR}/res")
set(PROJECT_SOURCE_PATH "${CMAKE_CURRENT_LIST_DIR}/src")
set(PROJECT_COPYWRITE "See ${PROJECT_HOMEPAGE_URL}")


#
# Set up the actual library
#
add_subdirectory("${PROJECT_SOURCE_PATH}")

#
# Add helper target for vs
#
add_custom_target( skse_package
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    COMMENT "Running CPack. Please wait..."
    COMMAND "${CMAKE_CPACK_COMMAND}" "--preset" "$<$<CONFIG:DEBUG>:skse-debug>$<$<CONFIG:RELEASE>:skse-release>"
    COMMAND ${CMAKE_COMMAND} -E echo "CPack Done. Cleaning up."
    COMMAND "${CMAKE_COMMAND}" -E rm "-r" "${CMAKE_CURRENT_LIST_DIR}/output/$<$<CONFIG:DEBUG>:Debug>$<$<CONFIG:RELEASE>:Release>/_CPack_Packages"
)

#
# And finally produce build artifacts
#
set(PLUGIN_LOADER_OUTPUT_DIR "Data/skse/Plugins/")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_LIST_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_RESOURCES_PATH}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION_MAJOR}_${CMAKE_PROJECT_VERSION_MINOR}_${CMAKE_PROJECT_VERSION_PATCH}")

install(
    CODE
    "message(\"CIP = ${CMAKE_INSTALL_PREFIX}\")"
)
install(
    FILES
        "${PROJECT_RESOURCES_PATH}/GCBugFix.ini"
    DESTINATION
        "${PLUGIN_LOADER_OUTPUT_DIR}"
)

install(
    TARGETS
        skse_output_dll
    RUNTIME
        DESTINATION "${PLUGIN_LOADER_OUTPUT_DIR}"
)

install(
    FILES
        "${CPACK_RESOURCE_FILE_README}"
        "${CPACK_RESOURCE_FILE_LICENSE}"
    DESTINATION "."
)

set(CPACK_GENERATOR "ZIP")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
