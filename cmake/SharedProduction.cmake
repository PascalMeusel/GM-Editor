# Compile production code in responsibility-oriented slices, then expose stable
# umbrella libraries to the application and compatibility tests.
set(GM_CORE_SOURCES ${MODEL_SOURCES})
set(GM_UI_SOURCES ${CONTROLLER_SOURCES} ${VIEW_SOURCES} ${APPLICATION_SOURCES})
set(GM_IMGUI_BRIDGE_SOURCE
    "${CMAKE_CURRENT_SOURCE_DIR}/Views/Editor/Widgets/UiInteractionTrace.cpp")
list(REMOVE_ITEM GM_UI_SOURCES "${GM_IMGUI_BRIDGE_SOURCE}")

set(GM_CORE_FOUNDATION_SOURCES)
set(GM_CORE_RULESET_SOURCES)
set(GM_CORE_CHARACTER_SOURCES)
set(GM_CORE_WORLD_MAP_SOURCES)
set(GM_CORE_SESSION_SOURCES)
set(GM_CORE_PROJECT_SOURCES)
set(GM_CORE_PERSISTENCE_SOURCES)
foreach(source IN LISTS GM_CORE_SOURCES)
    file(TO_CMAKE_PATH "${source}" source_path)
    if(source_path MATCHES "/Models/(Persistence)/" OR source_path MATCHES "/Models/DataBaseManager")
        list(APPEND GM_CORE_PERSISTENCE_SOURCES "${source}")
    elseif(source_path MATCHES "/Models/(Map|Media|Dungeon|World)/")
        list(APPEND GM_CORE_WORLD_MAP_SOURCES "${source}")
    elseif(source_path MATCHES "/Models/(Character|NPC|Combat)/")
        list(APPEND GM_CORE_CHARACTER_SOURCES "${source}")
    elseif(source_path MATCHES "/Models/(Inventory|Magic|Shop)/")
        list(APPEND GM_CORE_CHARACTER_SOURCES "${source}")
    elseif(source_path MATCHES "/Models/(Session|Audio|Story)/")
        list(APPEND GM_CORE_SESSION_SOURCES "${source}")
    elseif(source_path MATCHES "/Models/(Project|Settings)/")
        list(APPEND GM_CORE_PROJECT_SOURCES "${source}")
    elseif(source_path MATCHES "/Models/Rulesets/")
        list(APPEND GM_CORE_RULESET_SOURCES "${source}")
    else()
        list(APPEND GM_CORE_FOUNDATION_SOURCES "${source}")
    endif()
endforeach()

function(gm_add_core_slice target_name)
    if(NOT ARGN)
        message(FATAL_ERROR "Production slice ${target_name} has no sources")
    endif()
    add_library(${target_name} OBJECT ${ARGN})
    configure_ttrpg_sqlite_target(${target_name})
    target_compile_definitions(${target_name} PRIVATE
        GM_EDITOR_VERSION="${PROJECT_VERSION}")
endfunction()

gm_add_core_slice(gm_core_foundation ${GM_CORE_FOUNDATION_SOURCES})
gm_add_core_slice(gm_core_rulesets ${GM_CORE_RULESET_SOURCES})
gm_add_core_slice(gm_core_character ${GM_CORE_CHARACTER_SOURCES})
gm_add_core_slice(gm_core_world_map ${GM_CORE_WORLD_MAP_SOURCES})
gm_add_core_slice(gm_core_session ${GM_CORE_SESSION_SOURCES})
gm_add_core_slice(gm_core_project ${GM_CORE_PROJECT_SOURCES})
gm_add_core_slice(gm_core_persistence ${GM_CORE_PERSISTENCE_SOURCES})

if(NOT WIN32)
    foreach(image_dependency PNG JPEG)
        if(${image_dependency}_FOUND)
            target_compile_definitions(gm_core_world_map PRIVATE
                TTRPG_HAS_LIB${image_dependency}=1)
            if(TARGET ${image_dependency}::${image_dependency})
                target_link_libraries(gm_core_world_map PRIVATE
                    ${image_dependency}::${image_dependency})
            else()
                target_include_directories(gm_core_world_map PRIVATE
                    ${${image_dependency}_INCLUDE_DIRS})
            endif()
        endif()
    endforeach()
endif()

set(GM_CORE_SLICE_TARGETS
    gm_core_foundation
    gm_core_rulesets
    gm_core_character
    gm_core_world_map
    gm_core_session
    gm_core_project
    gm_core_persistence)
set(GM_CORE_OBJECTS)
foreach(slice IN LISTS GM_CORE_SLICE_TARGETS)
    list(APPEND GM_CORE_OBJECTS "$<TARGET_OBJECTS:${slice}>")
endforeach()

add_library(gm_core STATIC ${GM_CORE_OBJECTS})
find_package(Threads REQUIRED)
target_link_libraries(gm_core_session PRIVATE Threads::Threads)
target_link_libraries(gm_core_world_map PRIVATE Threads::Threads)
target_include_directories(gm_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gm_core PUBLIC SQLiteCpp Threads::Threads)
if(WIN32)
    target_link_libraries(gm_core PUBLIC gdiplus ole32 winmm comdlg32)
else()
    foreach(image_dependency PNG JPEG)
        if(${image_dependency}_FOUND)
            if(TARGET ${image_dependency}::${image_dependency})
                target_link_libraries(gm_core PUBLIC
                    ${image_dependency}::${image_dependency})
            else()
                target_link_libraries(gm_core PUBLIC
                    ${${image_dependency}_LIBRARIES})
            endif()
        endif()
    endforeach()
endif()

add_library(gm_imgui STATIC ${IMGUI_SOURCES} "${GM_IMGUI_BRIDGE_SOURCE}")
target_include_directories(gm_imgui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(gm_imgui PUBLIC
    "${TTRPG_IMGUI_DIR}" "${TTRPG_IMGUI_DIR}/backends")
target_link_libraries(gm_imgui PUBLIC glfw OpenGL::GL)

set(GM_UI_MAP_SOURCES)
set(GM_UI_COMBAT_SOURCES)
set(GM_UI_CHARACTER_SOURCES)
set(GM_UI_SESSION_SOURCES)
set(GM_UI_CONTENT_SOURCES)
set(GM_UI_SHELL_SOURCES)
foreach(source IN LISTS GM_UI_SOURCES)
    file(TO_CMAKE_PATH "${source}" source_path)
    if(source_path MATCHES "/(MapCreator|MapViewer)/" OR source_path MATCHES "/Widgets/Map/")
        list(APPEND GM_UI_MAP_SOURCES "${source}")
    elseif(source_path MATCHES "/(BattleTracker|BattlefieldCreator|EncounterBuilder)/")
        list(APPEND GM_UI_COMBAT_SOURCES "${source}")
    elseif(source_path MATCHES "/(CharacterSheet|PlayerCreator|PlayerOverview|NPCCreator|NPCOverview)/")
        list(APPEND GM_UI_CHARACTER_SOURCES "${source}")
    elseif(source_path MATCHES "/(AudioManager|AudioQuickControls|GMScreen|LiveSessionDashboard|LocalPlayerDisplay)/")
        list(APPEND GM_UI_SESSION_SOURCES "${source}")
    elseif(source_path MATCHES "/(InventoryManager|ItemCreator|SpellLibrary|StoryWorkspace|TabletopToolkit)/")
        list(APPEND GM_UI_CONTENT_SOURCES "${source}")
    else()
        list(APPEND GM_UI_SHELL_SOURCES "${source}")
    endif()
endforeach()

function(gm_add_ui_slice target_name)
    if(NOT ARGN)
        message(FATAL_ERROR "Editor slice ${target_name} has no sources")
    endif()
    add_library(${target_name} OBJECT ${ARGN})
    target_link_libraries(${target_name} PRIVATE gm_core gm_imgui)
    target_compile_definitions(${target_name} PRIVATE
        GM_EDITOR_VERSION="${PROJECT_VERSION}")
    configure_ttrpg_sqlite_target(${target_name})
endfunction()

gm_add_ui_slice(gm_ui_map ${GM_UI_MAP_SOURCES})
gm_add_ui_slice(gm_ui_combat ${GM_UI_COMBAT_SOURCES})
gm_add_ui_slice(gm_ui_character ${GM_UI_CHARACTER_SOURCES})
gm_add_ui_slice(gm_ui_session ${GM_UI_SESSION_SOURCES})
gm_add_ui_slice(gm_ui_content ${GM_UI_CONTENT_SOURCES})
gm_add_ui_slice(gm_ui_shell ${GM_UI_SHELL_SOURCES})

set(GM_UI_SLICE_TARGETS
    gm_ui_map gm_ui_combat gm_ui_character
    gm_ui_session gm_ui_content gm_ui_shell)
set(GM_UI_OBJECTS)
foreach(slice IN LISTS GM_UI_SLICE_TARGETS)
    list(APPEND GM_UI_OBJECTS "$<TARGET_OBJECTS:${slice}>")
endforeach()

add_library(gm_editor_ui STATIC ${GM_UI_OBJECTS})
target_link_libraries(gm_editor_ui PUBLIC gm_core gm_imgui)
if(WIN32)
    target_link_libraries(gm_editor_ui PUBLIC comdlg32)
endif()

# Existing controller-isolation tests intentionally define view/save doubles.
set(GM_UI_DOUBLE_TARGETS secondary_editor_persistence_tests battle_tracker_preparation_tests
    battle_tracker_wrapup_tests battle_tracker_turns_tests battle_tracker_table_break_tests)

# Normalize legacy standalone tools onto the same production binaries.
get_property(gm_product_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(product_target IN LISTS gm_product_targets)
    if(product_target STREQUAL "TTRPG_Editor")
        continue()
    endif()
    get_target_property(product_type ${product_target} TYPE)
    if(NOT product_type STREQUAL "EXECUTABLE")
        continue()
    endif()
    get_target_property(product_sources ${product_target} SOURCES)
    set(consumer_sources)
    set(uses_imgui FALSE)
    set(uses_ui FALSE)
    foreach(product_source IN LISTS product_sources)
        get_filename_component(absolute_source "${product_source}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        if(absolute_source IN_LIST GM_CORE_SOURCES)
            continue()
        elseif(absolute_source IN_LIST GM_UI_SOURCES AND
               NOT product_target IN_LIST GM_UI_DOUBLE_TARGETS)
            set(uses_ui TRUE)
            continue()
        elseif(absolute_source IN_LIST IMGUI_SOURCES OR
               absolute_source STREQUAL GM_IMGUI_BRIDGE_SOURCE)
            set(uses_imgui TRUE)
            continue()
        endif()
        list(APPEND consumer_sources "${product_source}")
    endforeach()
    set_property(TARGET ${product_target} PROPERTY SOURCES "${consumer_sources}")
    target_link_libraries(${product_target} PRIVATE gm_core)
    if(uses_ui)
        target_link_libraries(${product_target} PRIVATE gm_editor_ui)
    endif()
    if(uses_imgui)
        target_link_libraries(${product_target} PRIVATE gm_imgui)
    endif()
endforeach()

add_library(gm_header_checks OBJECT
    tools/header_checks/map_generation_types.cpp
    tools/header_checks/map_generation.cpp
    tools/header_checks/database_manager.cpp
    tools/header_checks/map_png_export_planning.cpp)
target_link_libraries(gm_header_checks PRIVATE gm_core)
