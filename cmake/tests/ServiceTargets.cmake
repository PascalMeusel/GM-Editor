    add_executable(schema_migration_tests
        tools/schema_migration_tests.cpp
        ${MODEL_SOURCES}
        Models/Media/RgbaImageLoader.cpp
    )
    add_executable(foundation_tests
        tools/foundation_tests.cpp
        ${MODEL_SOURCES}
        Models/Media/RgbaImageLoader.cpp
    )
    if(WIN32)
        # Explicit manual benchmark; the complete production persistence path
        # and generated fixtures are measured outside the CI test workload.
        add_executable(persistence_benchmark EXCLUDE_FROM_ALL
            tools/persistence_benchmark.cpp
            ${MODEL_SOURCES}
            Models/Media/RgbaImageLoader.cpp)
        configure_ttrpg_sqlite_target(persistence_benchmark)
        target_link_libraries(persistence_benchmark PRIVATE gdiplus ole32)
    endif()
    add_executable(player_safety_tests
        tools/player_safety_tests.cpp
        Models/Map/MapPngExport.cpp
        Models/Map/MapRenderPlan.cpp
        Models/Project/ApplicationPaths.cpp
        Models/Media/RgbaImageLoader.cpp
    )
    target_include_directories(player_safety_tests PRIVATE .)
    add_executable(ruleset_library_tests
        tools/ruleset_library_tests.cpp
        Models/Rulesets/RulesetLibrary.cpp
        Models/Rulesets/RulesetResolution.cpp
        Models/Rulesets/RulesetDefinitionSerialization.cpp
        Models/Rulesets/RulesetStudioLibrary.cpp
        Models/Rulesets/DefinitionBackedRuleSet.cpp
        Models/Rulesets/RulesetDefinitionLayering.cpp
        Models/Rulesets/RulesetDefinitionValidator.cpp
        Models/Rulesets/RulesetFormulaEvaluator.cpp
    )
    target_include_directories(ruleset_library_tests PRIVATE .)
    add_executable(localization_tests
        tools/localization_tests.cpp
        Models/Localization/TranslationCatalog.cpp
        Models/Localization/LocalizationSettings.cpp
        Models/Localization/LocalizationService.cpp
        Models/Localization/LocaleFormatting.cpp
        Models/Rulesets/RulesetDefinitionValidator.cpp
        Models/Rulesets/RulesetFormulaEvaluator.cpp
        Models/Rulesets/RulesetPresentationFormatter.cpp
        Models/Rulesets/RulesetDiagnosticFormatter.cpp
        Models/Project/ApplicationPaths.cpp
        Models/Settings/AccessibilitySettings.cpp
    )
    target_include_directories(localization_tests PRIVATE .)

    add_executable(application_paths_tests
        tools/application_paths_tests.cpp
        Models/Project/ApplicationPaths.cpp
    )
    target_include_directories(application_paths_tests PRIVATE .)
    target_compile_definitions(application_paths_tests PRIVATE GM_EDITOR_VERSION="${PROJECT_VERSION}")
    add_executable(crash_report_tests
        tools/crash_report_tests.cpp
        Models/Project/ApplicationPaths.cpp
        Models/Project/CrashReportService.cpp
    )
    target_include_directories(crash_report_tests PRIVATE .)
    target_compile_definitions(crash_report_tests PRIVATE GM_EDITOR_VERSION="${PROJECT_VERSION}")
    add_executable(audio_playback_tests
        tools/audio_playback_tests.cpp
        Models/Audio/AudioBackend.cpp
        Models/Audio/AudioPlaybackService.cpp
        Models/Audio/AudioPlaybackServiceState.cpp
    )
    target_include_directories(audio_playback_tests PRIVATE .)
    if(WIN32)
        target_link_libraries(audio_playback_tests PRIVATE winmm)
    endif()
    add_executable(starter_campaign_template_tests
        tools/starter_campaign_template_tests.cpp
        Models/Project/StarterCampaignTemplate.cpp
    )
    target_include_directories(starter_campaign_template_tests PRIVATE .)
    target_link_libraries(starter_campaign_template_tests PRIVATE gm_core)
    add_executable(project_save_settings_tests
        tools/project_save_settings_tests.cpp
        Models/Project/ProjectSaveSettings.cpp
    )
    target_include_directories(project_save_settings_tests PRIVATE .)
    add_executable(world_inspector_tests
        tools/world_inspector_tests.cpp
        Models/Project/WorldInspectorService.cpp
        Models/Project/WorldInspectorNations.cpp
        Models/Project/WorldInspectorShops.cpp
        Models/Project/WorldInspectorDungeons.cpp
    )
    target_include_directories(world_inspector_tests PRIVATE .)
    add_executable(visual_scenario_tests
        tools/visual_scenario_tests.cpp
        Models/UX/VisualScenarioCatalog.cpp
    )
    target_include_directories(visual_scenario_tests PRIVATE .)
    add_executable(map_atlas_tests
        tools/map_atlas_tests.cpp
        Models/Map/MapAtlasService.cpp
    )
    target_include_directories(map_atlas_tests PRIVATE .)
    add_executable(map_world_expansion_tests
        tools/map_world_expansion_tests.cpp
        Models/Map/MapWorldExpansion.cpp
    )
    target_include_directories(map_world_expansion_tests PRIVATE .)
    add_executable(map_package_tests
        tools/map_package_tests.cpp
        Models/Map/MapPackageService.cpp
        Models/Map/MapPackageDraftInstallService.cpp
        Models/Map/MapPackageDraftInstallInternal.cpp
        Models/Map/MapPackageDraftInstallRecovery.cpp
        Models/Map/MapPackageDraftInstallLifecycle.cpp
        Models/Project/ApplicationPaths.cpp
    )
    target_include_directories(map_package_tests PRIVATE .)
    add_executable(map_checkpoint_tests
        tools/map_checkpoint_tests.cpp
        Models/Map/MapCheckpointService.cpp
        Models/Map/MapPackageService.cpp
        Models/Project/ApplicationPaths.cpp
    )
    target_include_directories(map_checkpoint_tests PRIVATE .)
    add_executable(encounter_atlas_tests
        tools/encounter_atlas_tests.cpp
        Models/Combat/EncounterAtlasService.cpp
        Models/Map/MapAtlasService.cpp
    )
    target_include_directories(encounter_atlas_tests PRIVATE .)
    add_executable(travel_scene_tests
        tools/travel_scene_tests.cpp
        Models/Map/TravelSceneGenerator.cpp
        Models/Map/TravelSceneInternal.cpp
        Models/Map/TravelSceneWaterComposition.cpp
        Models/Map/TravelSceneRoadComposition.cpp
        Models/Map/TravelScenePurposeComposition.cpp
        Models/Map/TravelSceneFurnishings.cpp
        Models/Map/TacticalMapGenerator.cpp
        Models/Combat/EncounterAtlasService.cpp
        Models/Map/MapAtlasService.cpp
    )
    target_include_directories(travel_scene_tests PRIVATE .)
    add_executable(map_semantic_resolver_tests
        tools/map_semantic_resolver_tests.cpp
    )
    add_executable(map_generation_tests
        tools/map_generation_tests.cpp
    )
    add_executable(map_world_brief_v5_tests
        tools/map_world_brief_v5_tests.cpp
    )
    add_executable(map_v5_acceptance_tests
        tools/map_v5_acceptance_tests.cpp
    )
    target_include_directories(map_v5_acceptance_tests PRIVATE .)
    add_executable(map_stroke_authoring_tests tools/map_stroke_authoring_tests.cpp)
    target_include_directories(map_stroke_authoring_tests PRIVATE .)
    add_test(NAME map_stroke_authoring_tests COMMAND map_stroke_authoring_tests)
    add_executable(map_edit_transaction_tests
        tools/map_edit_transaction_tests.cpp
    )
    target_include_directories(map_edit_transaction_tests PRIVATE .)
    add_executable(map_preview_parity_tests
        tools/map_preview_parity_tests.cpp
        Models/Map/MapRenderPlan.cpp
    )
    target_include_directories(map_preview_parity_tests PRIVATE .)
    add_executable(map_illustrated_atlas_tests tools/map_illustrated_atlas_tests.cpp Models/Map/MapRenderPlan.cpp)
    target_include_directories(map_illustrated_atlas_tests PRIVATE .)
    add_executable(startup_diagnostics_tests tools/startup_diagnostics_tests.cpp Models/Project/StartupDiagnostics.cpp)
    target_include_directories(startup_diagnostics_tests PRIVATE .)
    add_executable(map_trace_export_tests
        tools/map_trace_export_tests.cpp Models/Map/MapPngExport.cpp Models/Map/MapRenderPlan.cpp
        Models/Project/ApplicationPaths.cpp Models/Media/RgbaImageLoader.cpp)
    target_include_directories(map_trace_export_tests PRIVATE .)
    add_executable(map_tactical_road_ink_tests
        tools/map_tactical_road_ink_tests.cpp Models/Map/MapPngExport.cpp Models/Map/MapRenderPlan.cpp
        Models/Project/ApplicationPaths.cpp Models/Media/RgbaImageLoader.cpp)
    target_include_directories(map_tactical_road_ink_tests PRIVATE .)
    add_executable(map_object_editing_preview
        tools/map_object_editing_preview.cpp Models/Map/MapPngExport.cpp Models/Map/MapRenderPlan.cpp
        Models/Project/ApplicationPaths.cpp Models/Media/RgbaImageLoader.cpp)
    target_include_directories(map_object_editing_preview PRIVATE .)
    add_executable(map_player_render_geometry_tests
        tools/map_player_render_geometry_tests.cpp
        Models/Map/MapRenderPlan.cpp
        Models/Localization/TranslationCatalog.cpp
        Models/Localization/LocalizationSettings.cpp
        Models/Localization/LocalizationService.cpp
        Models/Project/ApplicationPaths.cpp
        ${TTRPG_IMGUI_DIR}/imgui.cpp
        ${TTRPG_IMGUI_DIR}/imgui_draw.cpp
        ${TTRPG_IMGUI_DIR}/imgui_widgets.cpp
        ${TTRPG_IMGUI_DIR}/imgui_tables.cpp
    )
    target_include_directories(map_player_render_geometry_tests PRIVATE . ${TTRPG_IMGUI_DIR})
    add_executable(map_generated_feature_drag_tests
        tools/map_generated_feature_drag_tests.cpp
        Models/Map/MapRenderPlan.cpp
        ${TTRPG_IMGUI_DIR}/imgui.cpp
        ${TTRPG_IMGUI_DIR}/imgui_draw.cpp
        ${TTRPG_IMGUI_DIR}/imgui_widgets.cpp
        ${TTRPG_IMGUI_DIR}/imgui_tables.cpp
    )
    target_include_directories(map_generated_feature_drag_tests PRIVATE . ${TTRPG_IMGUI_DIR})
    add_executable(ui_prose_input_tests
        tools/ui_prose_input_tests.cpp
        ${TTRPG_IMGUI_DIR}/imgui.cpp
        ${TTRPG_IMGUI_DIR}/imgui_draw.cpp
        ${TTRPG_IMGUI_DIR}/imgui_widgets.cpp
        ${TTRPG_IMGUI_DIR}/imgui_tables.cpp
        ${TTRPG_IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    )
    target_include_directories(ui_prose_input_tests PRIVATE . ${TTRPG_IMGUI_DIR})
    add_test(NAME ui_prose_input_tests COMMAND ui_prose_input_tests)
    add_executable(ui_task_layout_tests
        tools/ui_task_layout_tests.cpp
        ${TTRPG_IMGUI_DIR}/imgui.cpp
        ${TTRPG_IMGUI_DIR}/imgui_draw.cpp
        ${TTRPG_IMGUI_DIR}/imgui_widgets.cpp
        ${TTRPG_IMGUI_DIR}/imgui_tables.cpp
    )
    target_include_directories(ui_task_layout_tests PRIVATE . ${TTRPG_IMGUI_DIR})
    add_test(NAME ui_task_layout_tests COMMAND ui_task_layout_tests)
    add_executable(ui_adaptive_fields_tests
        tools/ui_adaptive_fields_tests.cpp
        ${TTRPG_IMGUI_DIR}/imgui.cpp
        ${TTRPG_IMGUI_DIR}/imgui_draw.cpp
        ${TTRPG_IMGUI_DIR}/imgui_widgets.cpp
        ${TTRPG_IMGUI_DIR}/imgui_tables.cpp
        ${TTRPG_IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    )
    target_include_directories(ui_adaptive_fields_tests PRIVATE . ${TTRPG_IMGUI_DIR})
    add_test(NAME ui_adaptive_fields_tests COMMAND ui_adaptive_fields_tests)
    add_executable(map_image_trace_workflow_tests
        tools/map_image_trace_workflow_tests.cpp
    )
    target_include_directories(map_image_trace_workflow_tests PRIVATE .)
    add_executable(map_facts_ux_tests
        tools/map_facts_ux_tests.cpp
        Models/Localization/TranslationCatalog.cpp
    )
    target_include_directories(map_facts_ux_tests PRIVATE .)
    add_executable(map_legacy_migration_tests
        tools/map_legacy_migration_tests.cpp
        Models/Persistence/PersistenceSerialization.cpp
    )
    target_include_directories(map_legacy_migration_tests PRIVATE .)
    add_executable(map_image_import_service_tests
        tools/map_image_import_service_tests.cpp
        Models/Map/MapImageImportService.cpp
        Models/Map/MapImageImportInternal.cpp
        Models/Map/MapImageImportTrace.cpp
        Models/Map/MapImageImportLegacyReference.cpp
        Models/Map/MapRenderPlan.cpp
        Models/Project/ApplicationPaths.cpp
        Models/Project/ProjectAssetImportService.cpp
        Models/Project/ProjectAssetRepairService.cpp
        Models/Project/ProjectAssetService.cpp
        Models/Project/ProjectPackageLayout.cpp
        Models/Media/RgbaImageLoader.cpp
    )
    target_include_directories(map_image_import_service_tests PRIVATE .)
    target_compile_definitions(map_image_import_service_tests PRIVATE TTRPG_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
