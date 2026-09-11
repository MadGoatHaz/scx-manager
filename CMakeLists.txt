cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

##
## PROJECT
## name and version
##
project(scx-manager
        VERSION 1.16.0
        LANGUAGES CXX)


##
## INCLUDE
##
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(StandardProjectSettings)
include(CheckSymbolExists)
include(CompilerWarnings)
include(EnableCcache)
include(Linker)
include(StaticAnalyzers)
include(Sanitizers)
include(CPM)

find_package(Qt6 COMPONENTS Widgets LinguistTools REQUIRED)

# Mark fmt's own headers as SYSTEM includes so that its internal advisory
# -Wsuggest-final-{types,methods} diagnostics (format.h/format-inl.h) do not
# surface in the build. Project sources keep the full warning set; only the
# CPM-fetched dependency's headers are exempted.
set(FMT_SYSTEM_HEADERS ON)
CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 12.2.0
  EXCLUDE_FROM_ALL YES
)
CPMAddPackage(
  NAME Corrosion
  GITHUB_REPOSITORY corrosion-rs/corrosion
  GIT_TAG v0.6.1
  EXCLUDE_FROM_ALL YES
)

##
## CONFIGURATION
##
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${PROJECT_SOURCE_DIR}/src)

# Link this 'library' to set the c++ standard / compile-time options requested
add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_23)

# Files with translation sources. You should run lupdate manually when needed to regenerate them
file(GLOB SCXCTLUI_TS_FILES "${CMAKE_SOURCE_DIR}/lang/*.ts")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/lang")

set_source_files_properties(${SCXCTLUI_TS_FILES} PROPERTIES OUTPUT_LOCATION "${CMAKE_BINARY_DIR}/lang") # set qm files output directory
qt_add_translation(SCXCTLUI_QM_FILES ${SCXCTLUI_TS_FILES})   # generate qm files

add_custom_command(
    DEPENDS "${SCXCTLUI_QM_FILES}"
    OUTPUT "${CMAKE_BINARY_DIR}/scx_manager_locale.qrc"
    COMMAND cp "${CMAKE_SOURCE_DIR}/scx_manager_locale.qrc" "${CMAKE_BINARY_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

##
## Target
##
qt_add_executable(${PROJECT_NAME}
    src/main.cpp
    "${CMAKE_BINARY_DIR}/scx_manager_locale.qrc"
    scx_manager_resources.qrc
)
# Installed binary must find libscxctl-ui.so in <prefix>/lib (sibling of
# bin/). $ORIGIN/../lib is prefix-agnostic: works for /usr/local dev
# installs and /usr package (pacman/AUR) installs alike.
set_target_properties(scx-manager PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
add_library(scxctl-ui SHARED
    src/scx_utils.hpp src/scx_utils.cpp
    src/schedext-window-internal.hpp src/schedext-window-internal.cpp
    "${SCXCTLUI_INCLUDE_BUILD_DIR}/schedext-window.hpp" src/schedext-window.cpp
    src/schedext-window.ui
    src/scheduler-metadata.hpp src/scheduler-metadata.cpp
)
add_library(scxctl::scxctl-ui ALIAS scxctl-ui)
target_compile_definitions(scxctl-ui PRIVATE SCHEDEXT_LIB)

set_target_properties(${SCXCTLUI_TARGET_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)
target_include_directories(${SCXCTLUI_TARGET_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${SCXCTLUI_INCLUDE_BUILD_DIR}>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)
set_project_warnings(project_warnings)

# Add linker configuration
configure_linker(project_options)

# sanitizer options if supported by compiler
enable_sanitizers(project_options)

corrosion_import_crate(MANIFEST_PATH "scx-rustlib/Cargo.toml" FLAGS "${CARGO_FLAGS}")
corrosion_add_cxxbridge(scx-lib-cxxbridge CRATE scx_rustlib FILES lib.rs)

target_link_libraries(scxctl-ui PRIVATE project_warnings project_options Qt6::Widgets fmt::fmt-header-only scx-lib-cxxbridge)
target_link_libraries(${PROJECT_NAME} PRIVATE project_warnings project_options Qt6::Widgets scxctl::scxctl-ui)

option(ENABLE_UNITY "Enable Unity builds of projects" OFF)
if(ENABLE_UNITY)
   # Add for any project you want to apply unity builds for
   set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)
endif()

##
## Tests
##
enable_testing()
add_executable(test_scheduler_metadata
    tests/test_scheduler_metadata.cpp
    scx_manager_resources.qrc
)
target_include_directories(test_scheduler_metadata
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)
target_link_libraries(test_scheduler_metadata
    PRIVATE
    project_warnings
    project_options
    scxctl-ui
    Qt6::Core
)
add_test(NAME test_scheduler_metadata COMMAND test_scheduler_metadata)

# Offscreen check for the in-app scheduler info card (Chunk 2): drives the
# card fill API for every scheduler x profile combination in the metadata,
# and (when scx_loader is available on the host) the real window, verifying
# reactivity, placement, the fixed band height, and zero clipping.
# QT_QPA_PLATFORM=offscreen is forced both by the test itself and here.
add_executable(test_info_panel
    tests/test_info_panel.cpp
    scx_manager_resources.qrc
)
target_include_directories(test_info_panel
    PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    # The internal header pulls in the generated ui_schedext-window.h, which
    # lives in scxctl-ui's AUTOUIC directory (not exported to other targets).
    ${CMAKE_BINARY_DIR}/scxctl-ui_autogen/include
    # ...and scx_utils.hpp pulls in the corrosion-generated rust/cxx.h.
    ${CMAKE_BINARY_DIR}/corrosion_generated/cxxbridge/scx-lib-cxxbridge/include
)
target_link_libraries(test_info_panel
    PRIVATE
    project_warnings
    project_options
    scxctl-ui
    Qt6::Widgets
)
add_test(NAME test_info_panel COMMAND test_info_panel)
set_tests_properties(test_info_panel PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

##
## INSTALL
## install header files, generate and install cmake config files for find_package()
##
# use a custom package version config file instead of
# write_basic_package_version_file to ensure that it's architecture-independent
# https://github.com/nlohmann/json/issues/1697
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/scxctl-ui-config-version.cmake.in"
    ${SCXCTLUI_CMAKE_VERSION_CONFIG_FILE}
    @ONLY
)
configure_file(
    ${SCXCTLUI_CMAKE_CONFIG_TEMPLATE}
    ${SCXCTLUI_CMAKE_PROJECT_CONFIG_FILE}
    @ONLY
)

#write_basic_package_version_file(
#    ${SCXCTLUI_CMAKE_PROJECT_CONFIG_FILE}
#    VERSION ${PROJECT_VERSION}
#    COMPATIBILITY SameMajorVersion
#)

install(
    DIRECTORY ${SCXCTLUI_INCLUDE_BUILD_DIR}
    DESTINATION ${SCXCTLUI_INCLUDE_INSTALL_DIR}
)
install(
    FILES ${SCXCTLUI_CMAKE_PROJECT_CONFIG_FILE} ${SCXCTLUI_CMAKE_VERSION_CONFIG_FILE}
    DESTINATION ${SCXCTLUI_CONFIG_INSTALL_DIR}
)

export(
    TARGETS ${SCXCTLUI_TARGET_NAME}
    NAMESPACE scxctl::
    FILE ${SCXCTLUI_CMAKE_PROJECT_TARGETS_FILE}
)
install(
    TARGETS ${SCXCTLUI_TARGET_NAME}
    EXPORT ${SCXCTLUI_TARGETS_EXPORT_NAME}
    INCLUDES DESTINATION ${SCXCTLUI_INCLUDE_INSTALL_DIR}
)
install(
    EXPORT ${SCXCTLUI_TARGETS_EXPORT_NAME}
    NAMESPACE scxctl::
    DESTINATION "${SCXCTLUI_CONFIG_INSTALL_DIR}"
)

install(
   TARGETS ${PROJECT_NAME}
   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    FILES scx-manager.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(
    FILES ${CMAKE_SOURCE_DIR}/scx-manager.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)
