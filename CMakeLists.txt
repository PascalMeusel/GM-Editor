cmake_minimum_required(VERSION 3.21)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW) # Recommended for FetchContent timestamps
endif()

if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW) # Recommended for modern OpenGL finding
endif()

project(TTRPG_Editor_cpp VERSION 0.9.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source and execution text must not depend on the runner service account's
# Windows code page. ImGui and persisted localized text use UTF-8.
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/utf-8>)
endif()

# ImGui's optional observation hooks support scripted input against production
# widgets. Hooks remain disabled unless a QA observer explicitly owns a context.
add_compile_definitions(IMGUI_ENABLE_TEST_ENGINE)

include(FetchContent)
include(cmake/PatchedDependencies.cmake)

# --- GLFW ---
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${TTRPG_GLFW_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/glfw-build")

# --- OpenGL ---
find_package(OpenGL REQUIRED)

# --- SQLiteCpp ---
set(SQLITECPP_RUN_CPPCHECK OFF CACHE BOOL "" FORCE)
set(SQLITECPP_RUN_CPPLINT OFF CACHE BOOL "" FORCE)

# Tell CMake to handle SQLiteCpp's older CMakeLists with modern policy defaults.
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)

FetchContent_Declare(
    SQLiteCpp
    GIT_REPOSITORY https://github.com/SRombauts/SQLiteCpp.git
    GIT_TAG        4cb0bf68b75b2f8b749ec5dc77f152f8aa62ec0e # SQLiteCpp 3.3.0
)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/_vendor/SQLiteCpp/CMakeLists.txt")
    # The downloadable corresponding-source archive contains the exact dependency
    # used by the binary, so rebuilding that archive does not require fetching it.
    set(FETCHCONTENT_SOURCE_DIR_SQLITECPP "${CMAKE_CURRENT_SOURCE_DIR}/_vendor/SQLiteCpp")
endif()
FetchContent_MakeAvailable(SQLiteCpp)

include(cmake/ApplicationSources.cmake)

# Debug map translation units exceed COFF's default section limit. Apply to
# production code and its test consumers without changing optimization levels.
if(MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/bigobj>)
endif()

set(APP_RESOURCE_SOURCES)
if(WIN32)
    list(APPEND APP_RESOURCE_SOURCES TTRPG_Editor.rc)
endif()

set(IMGUI_SOURCES
    ${TTRPG_IMGUI_DIR}/imgui.cpp
    ${TTRPG_IMGUI_DIR}/imgui_draw.cpp
    ${TTRPG_IMGUI_DIR}/imgui_widgets.cpp
    ${TTRPG_IMGUI_DIR}/imgui_tables.cpp
    ${TTRPG_IMGUI_DIR}/imgui_demo.cpp
    ${TTRPG_IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    ${TTRPG_IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${TTRPG_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(TTRPG_Editor
    main.cpp
    ${APP_RESOURCE_SOURCES}
)

# Persistence is split into domain translation units and uses normal Release optimization.
set(GM_COPY_RESOURCE_DIRECTORY copy_directory)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
    set(GM_COPY_RESOURCE_DIRECTORY copy_directory_if_different)
endif()

add_custom_command(TARGET TTRPG_Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E ${GM_COPY_RESOURCE_DIRECTORY}
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Maps/Tilesets"
        "$<TARGET_FILE_DIR:TTRPG_Editor>/Resources/Maps/Tilesets"
    COMMAND ${CMAKE_COMMAND} -E ${GM_COPY_RESOURCE_DIRECTORY}
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts"
        "$<TARGET_FILE_DIR:TTRPG_Editor>/Resources/Fonts"
    COMMAND ${CMAKE_COMMAND} -E ${GM_COPY_RESOURCE_DIRECTORY}
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Portraits"
        "$<TARGET_FILE_DIR:TTRPG_Editor>/Resources/Portraits"
    COMMENT "Copying runtime map, font and portrait resources next to TTRPG_Editor"
)

function(configure_ttrpg_sqlite_target target_name)
    target_include_directories(${target_name} PRIVATE
        .
    )

    target_compile_definitions(${target_name} PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_ENABLE_COLUMN_METADATA
    )

    target_link_libraries(${target_name} PRIVATE
        SQLiteCpp
    )

    if(WIN32)
        target_link_libraries(${target_name} PRIVATE
            winmm
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
        target_link_libraries(${target_name} PRIVATE stdc++fs)
    endif()
endfunction()

# --- Includes ---
target_include_directories(TTRPG_Editor PRIVATE
    .
    ${TTRPG_IMGUI_DIR} 
    ${TTRPG_IMGUI_DIR}/backends
)

# --- Compile Definitions ---
target_compile_definitions(TTRPG_Editor PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_COLUMN_METADATA
    GM_EDITOR_VERSION="${PROJECT_VERSION}"
)

# --- Linking ---
target_link_libraries(TTRPG_Editor PRIVATE
    glfw
    OpenGL::GL
    SQLiteCpp
)

if(NOT WIN32)
    find_package(PNG QUIET)
    find_package(JPEG QUIET)
    if(PNG_FOUND)
        target_compile_definitions(TTRPG_Editor PRIVATE TTRPG_HAS_LIBPNG=1)
        if(TARGET PNG::PNG)
            target_link_libraries(TTRPG_Editor PRIVATE PNG::PNG)
        else()
            target_include_directories(TTRPG_Editor PRIVATE ${PNG_INCLUDE_DIRS})
            target_link_libraries(TTRPG_Editor PRIVATE ${PNG_LIBRARIES})
        endif()
    endif()
    if(JPEG_FOUND)
        target_compile_definitions(TTRPG_Editor PRIVATE TTRPG_HAS_LIBJPEG=1)
        if(TARGET JPEG::JPEG)
            target_link_libraries(TTRPG_Editor PRIVATE JPEG::JPEG)
        else()
            target_include_directories(TTRPG_Editor PRIVATE ${JPEG_INCLUDE_DIRS})
            target_link_libraries(TTRPG_Editor PRIVATE ${JPEG_LIBRARIES})
        endif()
    endif()
endif()

if(WIN32)
    target_link_libraries(TTRPG_Editor PRIVATE
        gdiplus
        ole32
        winmm
        comdlg32
    )

    set_target_properties(TTRPG_Editor PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(TTRPG_Editor PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(TTRPG_Editor PRIVATE stdc++fs)
endif()


include(cmake/SeedTargets.cmake)

if(TTRPG_BUILD_TESTS)
    include(cmake/tests/CoreTargets.cmake)
    include(cmake/tests/ServiceTargets.cmake)
    include(cmake/tests/MapQaTargets.cmake)
    include(cmake/tests/TestRegistration.cmake)
endif()

include(cmake/InstallPackaging.cmake)
include(cmake/WorkflowTestTargets.cmake)
include(cmake/DesktopQaTargets.cmake)
include(cmake/tests/CampaignExportTargets.cmake)
include(cmake/tests/AutosavePolicyTargets.cmake)
include(cmake/tests/EditHistoryTargets.cmake)
include(cmake/tests/SessionOutcomeTargets.cmake)
include(cmake/tests/DatabaseMigrationSafetyTargets.cmake)
include(cmake/tests/PlayerPublicationAssetsTargets.cmake)
include(cmake/tests/PlayerRecapPackageTargets.cmake)
include(cmake/tests/TextDraftRecoveryTargets.cmake)
include(cmake/tests/DiagnosticReportTargets.cmake)
include(cmake/tests/UvttExportTargets.cmake)
include(cmake/tests/CampaignMarkdownExchangeTargets.cmake)
include(cmake/tests/TravelSessionIntegrationTargets.cmake)
include(cmake/tests/CombatEditHistoryTargets.cmake)
include(cmake/TestIsolation.cmake)

include(cmake/SharedProduction.cmake)

# The shipped executable has an explicit dependency on the production UI
# boundary. Legacy standalone tools are normalized inside SharedProduction;
# new product targets should link gm_core or gm_editor_ui directly.
target_link_libraries(TTRPG_Editor PRIVATE gm_editor_ui)

add_executable(map_editing_ui_qa EXCLUDE_FROM_ALL tools/map_editing_ui_qa.cpp)
target_link_libraries(map_editing_ui_qa PRIVATE gm_editor_ui)
add_dependencies(map_editing_ui_qa TTRPG_Editor)
