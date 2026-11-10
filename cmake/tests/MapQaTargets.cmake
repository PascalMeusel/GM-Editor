    add_executable(map_generation_batch_review
        tools/map_generation_batch_review.cpp
    )
    add_executable(map_generation_preview_export
        tools/map_generation_preview_export.cpp
        Models/Map/MapPngExport.cpp
        Models/Map/MapRenderPlan.cpp
        Models/Map/TacticalMapGenerator.cpp
        Models/Map/TravelSceneGenerator.cpp
        Models/Map/TravelSceneInternal.cpp
        Models/Map/TravelSceneWaterComposition.cpp
        Models/Map/TravelSceneRoadComposition.cpp
        Models/Map/TravelScenePurposeComposition.cpp
        Models/Map/TravelSceneFurnishings.cpp
        Models/Project/ApplicationPaths.cpp
        Models/Media/RgbaImageLoader.cpp
    )
    add_executable(map_generation_release_qa
        tools/map_generation_release_qa.cpp
        Models/Map/TravelSceneGenerator.cpp
        Models/Map/TravelSceneInternal.cpp
        Models/Map/TravelSceneWaterComposition.cpp
        Models/Map/TravelSceneRoadComposition.cpp
        Models/Map/TravelScenePurposeComposition.cpp
        Models/Map/TravelSceneFurnishings.cpp
        Models/Map/TacticalMapGenerator.cpp
        Models/Persistence/PersistenceSerialization.cpp
    )

    configure_ttrpg_sqlite_target(schema_migration_tests)
    configure_ttrpg_sqlite_target(foundation_tests)
    configure_ttrpg_sqlite_target(map_semantic_resolver_tests)
    configure_ttrpg_sqlite_target(map_generation_tests)
    configure_ttrpg_sqlite_target(map_world_brief_v5_tests)
    configure_ttrpg_sqlite_target(map_edit_transaction_tests)
    configure_ttrpg_sqlite_target(map_generation_batch_review)
    configure_ttrpg_sqlite_target(map_generation_preview_export)
    configure_ttrpg_sqlite_target(map_generation_release_qa)
    if(WIN32)
        target_link_libraries(map_generation_preview_export PRIVATE gdiplus ole32)
        target_link_libraries(schema_migration_tests PRIVATE gdiplus ole32)
        target_link_libraries(foundation_tests PRIVATE gdiplus ole32)
        target_link_libraries(player_safety_tests PRIVATE gdiplus ole32)
        target_link_libraries(map_trace_export_tests PRIVATE gdiplus ole32)
        target_link_libraries(map_tactical_road_ink_tests PRIVATE gdiplus ole32)
        target_link_libraries(map_object_editing_preview PRIVATE gdiplus ole32)
        target_link_libraries(map_image_import_service_tests PRIVATE gdiplus ole32)
    elseif(PNG_FOUND)
        target_compile_definitions(map_generation_preview_export PRIVATE TTRPG_HAS_LIBPNG=1)
        target_compile_definitions(schema_migration_tests PRIVATE TTRPG_HAS_LIBPNG=1)
        target_compile_definitions(foundation_tests PRIVATE TTRPG_HAS_LIBPNG=1)
        target_compile_definitions(player_safety_tests PRIVATE TTRPG_HAS_LIBPNG=1)
        target_compile_definitions(map_trace_export_tests PRIVATE TTRPG_HAS_LIBPNG=1)
        target_compile_definitions(map_tactical_road_ink_tests PRIVATE TTRPG_HAS_LIBPNG=1)
        target_compile_definitions(map_image_import_service_tests PRIVATE TTRPG_HAS_LIBPNG=1)
        if(TARGET PNG::PNG)
            target_link_libraries(map_generation_preview_export PRIVATE PNG::PNG)
            target_link_libraries(schema_migration_tests PRIVATE PNG::PNG)
            target_link_libraries(foundation_tests PRIVATE PNG::PNG)
            target_link_libraries(player_safety_tests PRIVATE PNG::PNG)
            target_link_libraries(map_trace_export_tests PRIVATE PNG::PNG)
            target_link_libraries(map_tactical_road_ink_tests PRIVATE PNG::PNG)
            target_link_libraries(map_image_import_service_tests PRIVATE PNG::PNG)
        else()
            target_include_directories(map_generation_preview_export PRIVATE ${PNG_INCLUDE_DIRS})
            target_link_libraries(map_generation_preview_export PRIVATE ${PNG_LIBRARIES})
            target_include_directories(schema_migration_tests PRIVATE ${PNG_INCLUDE_DIRS})
            target_link_libraries(schema_migration_tests PRIVATE ${PNG_LIBRARIES})
            target_include_directories(foundation_tests PRIVATE ${PNG_INCLUDE_DIRS})
            target_link_libraries(foundation_tests PRIVATE ${PNG_LIBRARIES})
            target_include_directories(player_safety_tests PRIVATE ${PNG_INCLUDE_DIRS})
            target_link_libraries(player_safety_tests PRIVATE ${PNG_LIBRARIES})
            target_include_directories(map_trace_export_tests PRIVATE ${PNG_INCLUDE_DIRS})
            target_link_libraries(map_trace_export_tests PRIVATE ${PNG_LIBRARIES})
            target_include_directories(map_tactical_road_ink_tests PRIVATE ${PNG_INCLUDE_DIRS})
            target_link_libraries(map_tactical_road_ink_tests PRIVATE ${PNG_LIBRARIES})
            target_include_directories(map_image_import_service_tests PRIVATE ${PNG_INCLUDE_DIRS})
            target_link_libraries(map_image_import_service_tests PRIVATE ${PNG_LIBRARIES})
        endif()
    endif()
    if(NOT WIN32 AND JPEG_FOUND)
        foreach(image_target map_generation_preview_export schema_migration_tests foundation_tests player_safety_tests map_image_import_service_tests map_trace_export_tests)
            target_compile_definitions(${image_target} PRIVATE TTRPG_HAS_LIBJPEG=1)
            if(TARGET JPEG::JPEG)
                target_link_libraries(${image_target} PRIVATE JPEG::JPEG)
            else()
                target_include_directories(${image_target} PRIVATE ${JPEG_INCLUDE_DIRS})
                target_link_libraries(${image_target} PRIVATE ${JPEG_LIBRARIES})
            endif()
        endforeach()
    endif()

    add_custom_target(copy_map_tileset_resources_for_tests
        COMMAND ${CMAKE_COMMAND} -E ${GM_COPY_RESOURCE_DIRECTORY}
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Maps/Tilesets"
            "${CMAKE_CURRENT_BINARY_DIR}/Resources/Maps/Tilesets"
        COMMENT "Copying map tileset resources for tests"
    )
    add_dependencies(map_semantic_resolver_tests copy_map_tileset_resources_for_tests)
    add_dependencies(map_generation_tests copy_map_tileset_resources_for_tests)
    add_dependencies(map_world_brief_v5_tests copy_map_tileset_resources_for_tests)
    add_dependencies(map_v5_acceptance_tests copy_map_tileset_resources_for_tests)
    add_dependencies(map_generation_batch_review copy_map_tileset_resources_for_tests)
    add_dependencies(map_generation_preview_export copy_map_tileset_resources_for_tests)
    add_dependencies(map_generation_release_qa copy_map_tileset_resources_for_tests)
