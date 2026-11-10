    add_executable(collection_persistence_tests tools/collection_persistence_tests.cpp)
    target_link_libraries(collection_persistence_tests PRIVATE gm_core)
    add_test(NAME collection_persistence COMMAND collection_persistence_tests)
    enable_testing()

    # Integrate beside the other standalone semantic generation test targets.
add_executable(dungeon_v10_tests tools/dungeon_v10_tests.cpp)
target_include_directories(dungeon_v10_tests PRIVATE .)
add_test(NAME dungeon_v10_tests COMMAND dungeon_v10_tests)

add_executable(map_region_composition_tests tools/map_region_composition_tests.cpp)
target_include_directories(map_region_composition_tests PRIVATE .)
add_test(NAME map_region_composition_tests COMMAND map_region_composition_tests)

add_executable(settlement_v10_tests tools/settlement_v10_tests.cpp Models/Map/MapRenderPlan.cpp)
    target_include_directories(settlement_v10_tests PRIVATE .)
    add_test(NAME settlement_v10_tests COMMAND settlement_v10_tests)
    add_executable(settlement_v12_tests tools/settlement_v12_tests.cpp Models/Map/MapRenderPlan.cpp)
    target_include_directories(settlement_v12_tests PRIVATE .)
    add_test(NAME settlement_v12_tests COMMAND settlement_v12_tests)
    add_executable(underground_v12_tests tools/underground_v12_tests.cpp)
    target_include_directories(underground_v12_tests PRIVATE .)
    add_test(NAME underground_v12_tests COMMAND underground_v12_tests)
    add_executable(map_local_illustration_tests tools/map_local_illustration_tests.cpp Models/Map/MapRenderPlan.cpp)
    target_include_directories(map_local_illustration_tests PRIVATE .)
    add_test(NAME map_local_illustration_tests COMMAND map_local_illustration_tests)
    add_executable(tactical_v10_tests tools/tactical_v10_tests.cpp Models/Map/TacticalMapGenerator.cpp Models/Map/MapRenderPlan.cpp)
    target_include_directories(tactical_v10_tests PRIVATE .)
    add_test(NAME tactical_v10_tests COMMAND tactical_v10_tests)
    add_test(NAME tactical_v11_tests COMMAND tactical_v10_tests --v11)
    add_test(NAME tactical_v12_tests COMMAND tactical_v10_tests --v12)

    add_executable(encounter_map_render_tests tools/encounter_map_render_tests.cpp Models/Map/MapRenderPlan.cpp)
    configure_ttrpg_sqlite_target(encounter_map_render_tests)
    add_test(NAME encounter_map_render COMMAND encounter_map_render_tests)
    add_executable(encounter_draft_persistence_tests tools/encounter_draft_persistence_tests.cpp)
    configure_ttrpg_sqlite_target(encounter_draft_persistence_tests)
    add_test(NAME encounter_draft_persistence COMMAND encounter_draft_persistence_tests)
    add_executable(encounter_grid_tests tools/encounter_grid_tests.cpp Models/Combat/EncounterAtlasService.cpp Models/Map/MapAtlasService.cpp)
    configure_ttrpg_sqlite_target(encounter_grid_tests)
    add_test(NAME encounter_grid COMMAND encounter_grid_tests)
    add_executable(encounter_canvas_tests tools/encounter_canvas_tests.cpp)
    target_include_directories(encounter_canvas_tests PRIVATE .)
    add_test(NAME encounter_canvas COMMAND encounter_canvas_tests)
    # Actual secondary controllers with fault-injected persistence and headless
    # view doubles. Do not link the production save service or views here.
    add_executable(secondary_editor_persistence_tests
        tools/secondary_editor_persistence_tests.cpp
        Controllers/Editor/Widgets/NPCOverview/NPCOverviewController.cpp
        Controllers/Editor/Widgets/NPCOverview/NPCOverviewAffiliations.cpp
        Controllers/Editor/Widgets/NPCOverview/NPCOverviewDeity.cpp
        Controllers/Editor/Widgets/NPCOverview/NPCOverviewFaction.cpp
        Controllers/Editor/Widgets/NPCOverview/NPCOverviewLifecycle.cpp
        Controllers/Editor/Widgets/NPCOverview/NPCOverviewMonster.cpp
        Controllers/Editor/Widgets/NPCOverview/NPCOverviewNpcMagic.cpp
        Controllers/Editor/Widgets/NPCOverview/NPCOverviewNpcProfile.cpp
        Controllers/Editor/Widgets/NPCOverview/NPCOverviewPersistence.cpp
        Controllers/Editor/Widgets/BattlefieldCreator/BattlefieldCreatorController.cpp
        Controllers/Editor/Widgets/BattlefieldCreator/BattlefieldCreatorHistory.cpp
        Controllers/Editor/Widgets/BattlefieldCreator/BattlefieldCreatorTerrain.cpp
        Models/Map/MapAtlasService.cpp)
    target_include_directories(secondary_editor_persistence_tests PRIVATE . ${TTRPG_IMGUI_DIR} ${TTRPG_GLFW_DIR}/include)
    if(MSVC)
        target_compile_options(secondary_editor_persistence_tests PRIVATE /Gy)
        target_link_options(secondary_editor_persistence_tests PRIVATE /OPT:REF)
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(secondary_editor_persistence_tests PRIVATE -ffunction-sections -fdata-sections)
        if(APPLE)
            target_link_options(secondary_editor_persistence_tests PRIVATE -Wl,-dead_strip)
        else()
            target_link_options(secondary_editor_persistence_tests PRIVATE -Wl,--gc-sections)
        endif()
    endif()
    add_test(NAME secondary_editor_persistence COMMAND secondary_editor_persistence_tests)
    add_executable(battle_tracker_preparation_tests
        tools/battle_tracker_preparation_tests.cpp
        Controllers/Editor/Widgets/BattleTracker/BattleTrackerSelection.cpp
        Controllers/Editor/Widgets/EncounterBuilder/EncounterBuilderPersistence.cpp)
    target_include_directories(battle_tracker_preparation_tests PRIVATE . ${TTRPG_IMGUI_DIR} ${TTRPG_GLFW_DIR}/include)
    add_test(NAME battle_tracker_preparation COMMAND battle_tracker_preparation_tests)

target_sources(battle_tracker_preparation_tests PRIVATE
    Controllers/Editor/Widgets/BattleTracker/BattleTrackerTableBreak.cpp
    Models/Session/SessionControlService.cpp
    Models/Session/InWorldTimeService.cpp
    Models/Session/InWorldTimePresentation.cpp)
target_sources(battle_tracker_preparation_tests PRIVATE
    Models/Audio/AudioPlaybackService.cpp
    Models/Audio/AudioPlaybackServiceState.cpp
    Models/Audio/AudioBackend.cpp)
if(WIN32)
 target_link_libraries(battle_tracker_preparation_tests PRIVATE winmm)
endif()


    add_executable(battle_tracker_wrapup_tests
        tools/battle_tracker_wrapup_tests.cpp
        Controllers/Editor/Widgets/BattleTracker/BattleTrackerWrapUp.cpp
        Controllers/Editor/Widgets/BattleTracker/BattleTrackerSelection.cpp
        Models/Audio/AudioTriggerService.cpp
        Models/Audio/AudioPlaybackService.cpp
        Models/Audio/AudioPlaybackServiceState.cpp
        Models/Audio/AudioBackend.cpp)
    target_include_directories(battle_tracker_wrapup_tests PRIVATE . ${TTRPG_IMGUI_DIR} ${TTRPG_GLFW_DIR}/include)
    if(WIN32)
        target_link_libraries(battle_tracker_wrapup_tests PRIVATE winmm)
    endif()
    add_test(NAME battle_tracker_wrapup COMMAND battle_tracker_wrapup_tests)

target_sources(battle_tracker_wrapup_tests PRIVATE
    Controllers/Editor/Widgets/BattleTracker/BattleTrackerTableBreak.cpp
    Models/Session/SessionControlService.cpp
    Models/Session/InWorldTimeService.cpp
    Models/Session/InWorldTimePresentation.cpp)


    add_executable(battle_tracker_turns_tests
        tools/battle_tracker_turns_tests.cpp
        Controllers/Editor/Widgets/BattleTracker/BattleTrackerTurns.cpp
        Controllers/Editor/Widgets/BattleTracker/BattleTrackerSelection.cpp
        Models/Audio/AudioTriggerService.cpp
        Models/Audio/AudioPlaybackService.cpp
        Models/Audio/AudioPlaybackServiceState.cpp
        Models/Audio/AudioBackend.cpp)
    target_include_directories(battle_tracker_turns_tests PRIVATE . ${TTRPG_IMGUI_DIR} ${TTRPG_GLFW_DIR}/include)
    if(WIN32)
        target_link_libraries(battle_tracker_turns_tests PRIVATE winmm)
    endif()
    add_test(NAME battle_tracker_turns COMMAND battle_tracker_turns_tests)

add_executable(battle_tracker_table_break_tests
    tools/battle_tracker_table_break_tests.cpp
    Controllers/Editor/Widgets/BattleTracker/BattleTrackerSelection.cpp
    Controllers/Editor/Widgets/BattleTracker/BattleTrackerTableBreak.cpp
    Models/Session/SessionControlService.cpp
    Models/Session/InWorldTimeService.cpp
    Models/Session/InWorldTimePresentation.cpp
    Models/Audio/AudioPlaybackService.cpp
    Models/Audio/AudioPlaybackServiceState.cpp
    Models/Audio/AudioBackend.cpp)
target_include_directories(battle_tracker_table_break_tests PRIVATE . ${TTRPG_IMGUI_DIR} ${TTRPG_GLFW_DIR}/include)
if(WIN32)
 target_link_libraries(battle_tracker_table_break_tests PRIVATE winmm)
endif()
add_test(NAME battle_tracker_table_break COMMAND battle_tracker_table_break_tests)


target_sources(battle_tracker_turns_tests PRIVATE
    Controllers/Editor/Widgets/BattleTracker/BattleTrackerTableBreak.cpp
    Models/Session/SessionControlService.cpp
    Models/Session/InWorldTimeService.cpp
    Models/Session/InWorldTimePresentation.cpp)





    add_executable(campaign_entity_draft_tests tools/campaign_entity_draft_tests.cpp)
    target_include_directories(campaign_entity_draft_tests PRIVATE .)
    add_test(NAME campaign_entity_draft_tests COMMAND campaign_entity_draft_tests)

    add_executable(map_structured_creation_tests tools/map_structured_creation_tests.cpp)
    target_include_directories(map_structured_creation_tests PRIVATE .)
    add_test(NAME map_structured_creation_tests COMMAND map_structured_creation_tests)

    add_executable(map_world_landform_tests tools/map_world_landform_tests.cpp)
    configure_ttrpg_sqlite_target(map_world_landform_tests)
    add_test(NAME map_world_landform_tests COMMAND map_world_landform_tests)

    add_executable(cave_v11_tests tools/cave_v11_tests.cpp Models/Map/MapRenderPlan.cpp)
    configure_ttrpg_sqlite_target(cave_v11_tests)
    add_test(NAME cave_v11_tests COMMAND cave_v11_tests)

    add_executable(settlement_v11_tests tools/settlement_v11_tests.cpp Models/Map/MapRenderPlan.cpp)
    configure_ttrpg_sqlite_target(settlement_v11_tests)
    add_test(NAME settlement_v11_tests COMMAND settlement_v11_tests)

    add_executable(dungeon_v11_tests tools/dungeon_v11_tests.cpp Models/Map/MapRenderPlan.cpp)
    configure_ttrpg_sqlite_target(dungeon_v11_tests)
    add_test(NAME dungeon_v11_tests COMMAND dungeon_v11_tests)

    add_executable(travel_v11_tests
        tools/travel_v11_tests.cpp
        Models/Map/TravelSceneGenerator.cpp
        Models/Map/TravelSceneInternal.cpp
        Models/Map/TravelSceneWaterComposition.cpp
        Models/Map/TravelSceneRoadComposition.cpp
        Models/Map/TravelScenePurposeComposition.cpp
        Models/Map/TravelSceneFurnishings.cpp
        Models/Map/MapRenderPlan.cpp)
    configure_ttrpg_sqlite_target(travel_v11_tests)
    add_test(NAME travel_v11_tests COMMAND travel_v11_tests)
    target_sources(battle_tracker_preparation_tests PRIVATE Controllers/Editor/Widgets/EncounterBuilder/EncounterBuilderDraft.cpp)

    add_executable(encounter_resume_state_tests tools/encounter_resume_state_tests.cpp)
    target_include_directories(encounter_resume_state_tests PRIVATE .)
    add_test(NAME encounter_resume_state COMMAND encounter_resume_state_tests)

    add_executable(map_travel_creation_tests
        tools/map_travel_creation_tests.cpp
        Models/Map/TravelSceneGenerator.cpp
        Models/Map/TravelSceneInternal.cpp
        Models/Map/TravelSceneWaterComposition.cpp
        Models/Map/TravelSceneRoadComposition.cpp
        Models/Map/TravelScenePurposeComposition.cpp
        Models/Map/TravelSceneFurnishings.cpp)
    target_include_directories(map_travel_creation_tests PRIVATE .)
target_sources(map_travel_creation_tests PRIVATE Models/Persistence/PersistenceSerialization.cpp)
    add_test(NAME map_travel_creation COMMAND map_travel_creation_tests)


    add_executable(session_plan_tests
        tools/session_plan_tests.cpp
        Models/Session/SessionPlanService.cpp
        Models/Session/SessionPlanInternal.cpp
        Models/Session/SessionPlanRuntime.cpp
        Models/Session/SessionPlanClock.cpp
        Models/Session/SessionPlanSerialization.cpp
        Models/Session/SessionControlService.cpp
        Models/Session/InWorldTimeService.cpp
        Models/Session/InWorldTimePresentation.cpp
        Models/Story/SceneLaunchService.cpp
        Models/Localization/LocalizationService.cpp
        Models/Localization/LocalizationSettings.cpp
        Models/Localization/TranslationCatalog.cpp
        Models/Localization/LocaleFormatting.cpp
        Models/Project/ApplicationPaths.cpp
    )
    target_include_directories(session_plan_tests PRIVATE .)
    add_test(NAME session_plan_tests COMMAND session_plan_tests)
