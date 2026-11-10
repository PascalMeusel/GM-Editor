    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/schema-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/foundation-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/player-safety-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-semantic-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-semantic-exe-resource-cwd-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-world-brief-v5-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-no-art-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-cave-progress-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-case-list-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-fixture-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tileset-source-qa-format-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/contextual-human-review-packet-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tileset-manifest-test")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime-tileset-bundle-test")
    add_test(NAME schema_migration_tests COMMAND schema_migration_tests)
    set_tests_properties(schema_migration_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/schema-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME foundation_tests COMMAND foundation_tests)
    set_tests_properties(foundation_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/foundation-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME player_safety_tests COMMAND player_safety_tests)
    set_tests_properties(player_safety_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/player-safety-test"
    )
    add_test(NAME ruleset_library_tests COMMAND ruleset_library_tests)
    add_test(NAME localization_tests COMMAND localization_tests)
    add_test(NAME focused_module_size_contract
        COMMAND ${CMAKE_COMMAND}
            -DGM_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -DGM_MAX_LINES=400
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckFocusedModuleSize.cmake
    )
    add_test(NAME production_source_manifest_contract
        COMMAND ${CMAKE_COMMAND}
            -DGM_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckProductionSourceManifest.cmake
    )
    add_test(NAME editor_architecture_contract
        COMMAND ${CMAKE_COMMAND}
            -DGM_SOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckEditorArchitecture.cmake
    )
    add_test(NAME localized_imgui_literal_gate
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-localized-imgui-literals.ps1"
    )
    add_test(NAME localized_imgui_literal_gate_controls
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-localized-imgui-literals.ps1"
    )
    add_test(NAME release_package_preflight_tests
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-release-package-preflight.ps1"
    )
    add_test(NAME application_paths_tests COMMAND application_paths_tests)
    add_test(NAME world_inspector_tests COMMAND world_inspector_tests)
    add_test(NAME visual_scenario_tests COMMAND visual_scenario_tests)
    add_test(NAME release_demo_de_self_test COMMAND seed_release_demo_de --self-test)
    add_test(NAME map_atlas_tests COMMAND map_atlas_tests)
    add_test(NAME map_world_expansion_tests COMMAND map_world_expansion_tests)
    add_test(NAME map_package_tests COMMAND map_package_tests)
    add_test(NAME map_checkpoint_tests COMMAND map_checkpoint_tests)
    add_test(NAME map_preview_parity_tests COMMAND map_preview_parity_tests)
    add_test(NAME map_player_render_geometry_tests COMMAND map_player_render_geometry_tests)
    add_test(NAME map_generated_feature_drag_tests COMMAND map_generated_feature_drag_tests)
    add_test(NAME map_illustrated_atlas_tests COMMAND map_illustrated_atlas_tests)
    add_test(NAME startup_diagnostics_tests COMMAND startup_diagnostics_tests)
    add_test(NAME map_trace_export_tests COMMAND map_trace_export_tests)
    add_test(NAME map_tactical_road_ink_tests COMMAND map_tactical_road_ink_tests)
    add_test(NAME map_image_trace_workflow_tests COMMAND map_image_trace_workflow_tests)
    add_test(NAME map_facts_ux_tests COMMAND map_facts_ux_tests)
    add_test(NAME map_legacy_migration_tests COMMAND map_legacy_migration_tests)
    add_test(NAME map_image_import_service_tests COMMAND map_image_import_service_tests)
    add_test(NAME map_edit_transaction_tests COMMAND map_edit_transaction_tests)
    add_test(NAME encounter_atlas_tests COMMAND encounter_atlas_tests)
    add_test(NAME travel_scene_tests COMMAND travel_scene_tests)
    add_test(NAME map_semantic_resolver_tests COMMAND map_semantic_resolver_tests)
    set_tests_properties(map_semantic_resolver_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-semantic-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME map_semantic_resolver_tests_exe_resource_cwd COMMAND map_semantic_resolver_tests)
    set_tests_properties(map_semantic_resolver_tests_exe_resource_cwd PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-semantic-exe-resource-cwd-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME map_generation_tests COMMAND map_generation_tests)
    set_tests_properties(map_generation_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME map_world_brief_v5_tests COMMAND map_world_brief_v5_tests)
    set_tests_properties(map_world_brief_v5_tests PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-world-brief-v5-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME map_v5_acceptance_smoke COMMAND map_v5_acceptance_tests --smoke)
    add_test(NAME map_generation_release_qa_smoke
        COMMAND map_generation_release_qa --tier pr --maps 25 --quality-attempts 12 --determinism-rate 5 --out "${CMAKE_CURRENT_BINARY_DIR}/map-generation-release-qa-smoke"
    )
    set_tests_properties(map_generation_release_qa_smoke PROPERTIES TIMEOUT 300)
    add_test(NAME map_generation_release_qa_v5_contract
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-map-generation-release-qa-contract.ps1"
            -ReportDirectory "${CMAKE_CURRENT_BINARY_DIR}/map-generation-release-qa-smoke"
            -ExpectedMapCount 25 -ExpectedAttempts 12
    )
    set_tests_properties(map_generation_release_qa_v5_contract PROPERTIES
        DEPENDS map_generation_release_qa_smoke
        TIMEOUT 60
    )
    add_test(NAME map_generation_qa_script_hardening
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-map-generation-qa-hardening.ps1"
            -RepositoryRoot "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    set_tests_properties(map_generation_qa_script_hardening PROPERTIES TIMEOUT 120)
    add_test(NAME fantasy_neutral_v3_asset_qa_smoke
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-fantasy-neutral-v3-asset-qa.ps1"
            -MapCount 10 -OutputRoot "${CMAKE_CURRENT_BINARY_DIR}/fantasy-neutral-v3-asset-qa-smoke"
    )
    set_tests_properties(fantasy_neutral_v3_asset_qa_smoke PROPERTIES TIMEOUT 120)
    add_test(NAME fantasy_neutral_v3_asset_qa_dashboard
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-fantasy-neutral-v3-asset-qa.ps1"
            -ReportPath "${CMAKE_CURRENT_BINARY_DIR}/fantasy-neutral-v3-asset-qa-smoke"
            -DashboardPath "${CMAKE_CURRENT_BINARY_DIR}/fantasy-neutral-v3-asset-qa-smoke"
            -ExpectedMapCount 10 -ExpectedAssetCount 424
    )
    set_tests_properties(fantasy_neutral_v3_asset_qa_dashboard PROPERTIES DEPENDS fantasy_neutral_v3_asset_qa_smoke)
    add_test(NAME fantasy_neutral_v3_review_gates
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-fantasy-neutral-v3-review-gates.ps1"
            -ReportRoot "${CMAKE_CURRENT_BINARY_DIR}/fantasy-neutral-v3-asset-qa-smoke"
            -OutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/fantasy-neutral-v3-review-gates"
            -ExpectedMapCount 10
    )
    set_tests_properties(fantasy_neutral_v3_review_gates PROPERTIES DEPENDS fantasy_neutral_v3_asset_qa_smoke)
    add_test(NAME map_generation_batch_review_smoke
        COMMAND map_generation_batch_review --cycles 1 --start-cycle 0 --art-readiness --skip-contact-sheet --out "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-test"
    )
    set_tests_properties(map_generation_batch_review_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME map_generation_batch_review_smoke_candidate_split
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-contextual-coverage-samples.ps1"
            -QaReportPath "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-test/map_generation_batch_report.json"
            -AssertCandidatePreviewRuntimeSplit
    )
    set_tests_properties(map_generation_batch_review_smoke_candidate_split PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
        DEPENDS map_generation_batch_review_smoke
    )
    add_test(NAME map_generation_batch_review_no_art_smoke
        COMMAND map_generation_batch_review --cycles 1 --start-cycle 0 --skip-contact-sheet --case-filter balanced --out "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-no-art-test"
    )
    set_tests_properties(map_generation_batch_review_no_art_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-no-art-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
        FAIL_REGULAR_EXPRESSION "Contextual style registry warm-up|Contextual runtime coverage"
    )
    add_test(NAME map_generation_batch_review_no_art_fast_path
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-batch-review-no-art-fast-path.ps1"
            -QaReportPath "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-no-art-test/map_generation_batch_report.json"
    )
    set_tests_properties(map_generation_batch_review_no_art_fast_path PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-no-art-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
        DEPENDS map_generation_batch_review_no_art_smoke
    )
    add_test(NAME map_generation_batch_review_cave_progress
        COMMAND map_generation_batch_review --cycles 1 --start-cycle 0 --skip-contact-sheet --case-filter cave_network --out "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-cave-progress-test"
    )
    set_tests_properties(map_generation_batch_review_cave_progress PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-cave-progress-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
        PASS_REGULAR_EXPRESSION "generation 3/6: Connecting cave tunnels"
    )
    add_test(NAME map_generation_batch_review_list_cases
        COMMAND map_generation_batch_review --list-cases --cycles 1 --include-context-fixtures --case-filter GRASS_WAY_FIXTURE
    )
    set_tests_properties(map_generation_batch_review_list_cases PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-case-list-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME map_generation_batch_review_case_slice_smoke
        COMMAND map_generation_batch_review --cycles 1 --start-cycle 0 --case-offset 1 --case-limit 2 --skip-contact-sheet --out "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-case-slice-test"
    )
    set_tests_properties(map_generation_batch_review_case_slice_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
        PASS_REGULAR_EXPRESSION "Case slice: offset 1, limit 2 from 8 filtered cases"
    )
    add_test(NAME map_generation_batch_review_case_slice_report
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-batch-review-case-slice.ps1"
            -QaReportPath "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-case-slice-test/map_generation_batch_report.json"
            -ExpectedAvailableCaseCount 8
            -ExpectedCaseOffset 1
            -ExpectedCaseLimit 2
            -ExpectedCases "mountain_ranges_0,individual_peaks_0"
            -AssertProgressFile
    )
    set_tests_properties(map_generation_batch_review_case_slice_report PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
        DEPENDS map_generation_batch_review_case_slice_smoke
    )
    add_test(NAME contextual_way_owner_sample_packet_list_cases
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/contextual_way_owner_sample_packet.ps1"
            -Cycles 1 -IncludeContextFixtures -CaseFilter GRASS_WAY_FIXTURE -ListCasesOnly
            -BatchReviewExe "$<TARGET_FILE:map_generation_batch_review>"
    )
    set_tests_properties(contextual_way_owner_sample_packet_list_cases PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME contextual_way_owner_sample_packet_case_slice_list_cases
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/contextual_way_owner_sample_packet.ps1"
            -Cycles 1 -CaseOffset 1 -CaseLimit 2 -ListCasesOnly
            -BatchReviewExe "$<TARGET_FILE:map_generation_batch_review>"
    )
    set_tests_properties(contextual_way_owner_sample_packet_case_slice_list_cases PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
        PASS_REGULAR_EXPRESSION "mountain_ranges_0"
    )
    add_test(NAME contextual_way_owner_sample_packet_case_slice_smoke
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/contextual_way_owner_sample_packet.ps1"
            -Cycles 1 -CaseOffset 1 -CaseLimit 1
            -BatchReviewExe "$<TARGET_FILE:map_generation_batch_review>"
            -IntermediateStem "case_slice_smoke"
            -BatchOutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-case-slice-wrapper-test"
            -PacketOutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/contextual-packet-case-slice-test"
            -PacketStem "contextual_human_review_packet_case_slice_test"
    )
    set_tests_properties(contextual_way_owner_sample_packet_case_slice_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
        PASS_REGULAR_EXPRESSION "Batch progress:"
    )
    add_test(NAME tileset_source_qa_format
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-source-qa-format.ps1"
    )
    set_tests_properties(tileset_source_qa_format PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tileset-source-qa-format-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME contextual_human_review_packet_latest_validation
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-human-review-packet-latest-validation.ps1"
    )
    set_tests_properties(contextual_human_review_packet_latest_validation PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/contextual-human-review-packet-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME map_generation_batch_review_grass_fixture_smoke
        COMMAND map_generation_batch_review --cycles 1 --start-cycle 0 --art-readiness --include-context-fixtures --case-filter grass_way_fixture --skip-contact-sheet --out "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-fixture-test"
    )
    set_tests_properties(map_generation_batch_review_grass_fixture_smoke PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-fixture-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME map_generation_batch_review_grass_fixture_samples
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-contextual-coverage-samples.ps1"
            -QaReportPath "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-fixture-test/map_generation_batch_report.json"
            -RequiredWorkOrderIds fn_v2_way_dirt_road_on_grass_open,fn_v2_way_trail_narrow_on_grass_open
            -ExpectedTargetAssetCount 590
            -ExpectedAcceptedAssetCount 92
            -ExpectedCandidateAssetCount 582
            -AssertCandidatePreviewRuntimeSplit
    )
    set_tests_properties(map_generation_batch_review_grass_fixture_samples PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/map-generation-review-fixture-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
        DEPENDS map_generation_batch_review_grass_fixture_smoke
    )
    add_test(NAME fantasy_neutral_v2_manifest_validation
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/validate-tileset-manifest.ps1"
            -SourceRoot "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Maps/Tilesets/Source/FantasyNeutral/v2"
            -OutputPath "${CMAKE_CURRENT_BINARY_DIR}/tileset-manifest-test/manifest_validation.json"
            -FailOnIssues
    )
    set_tests_properties(fantasy_neutral_v2_manifest_validation PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tileset-manifest-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME runtime_tileset_resource_bundle
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/assert-runtime-tileset-bundle.ps1"
            -BundleRoot "${CMAKE_CURRENT_BINARY_DIR}/Resources/Maps/Tilesets"
    )
    set_tests_properties(runtime_tileset_resource_bundle PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/runtime-tileset-bundle-test"
        ENVIRONMENT "TTRPG_DB_PATH=TTRPG.db"
    )
    add_test(NAME crash_report_tests COMMAND crash_report_tests)
    add_test(NAME audio_playback_tests COMMAND audio_playback_tests)
    add_test(NAME starter_campaign_template_tests COMMAND starter_campaign_template_tests)
    add_test(NAME project_save_settings_tests COMMAND project_save_settings_tests)
    add_test(NAME release_evidence_contract_tests
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/test-release-evidence-contract.ps1"
            -RepositoryRoot "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    set_tests_properties(release_evidence_contract_tests PROPERTIES TIMEOUT 60)
