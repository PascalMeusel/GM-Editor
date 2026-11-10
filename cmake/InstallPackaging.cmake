# Steam consumes an unpacked depot rather than an installer. These install
# rules deliberately include runtime assets only: no source-generation sheets,
# review candidates, developer databases, backups, or campaign workspaces.
install(TARGETS TTRPG_Editor RUNTIME DESTINATION .)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/distribution/gm_editor.install.json"
    DESTINATION .)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Audio"
    DESTINATION Resources)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Samples"
    DESTINATION Resources)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Fonts"
    DESTINATION Resources)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Portraits"
    DESTINATION Resources)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Maps/Tilesets/Styles"
    DESTINATION Resources/Maps/Tilesets)
set(FANTASY_NEUTRAL_V2_RUNTIME_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Maps/Tilesets/Generated/FantasyNeutral/v2")
install(FILES "${FANTASY_NEUTRAL_V2_RUNTIME_ROOT}/packed_runtime_manifest.json"
    DESTINATION Resources/Maps/Tilesets/Generated/FantasyNeutral/v2)
install(FILES
    "${FANTASY_NEUTRAL_V2_RUNTIME_ROOT}/atlases/fantasy_neutral__world__way_segment__page002.png"
    "${FANTASY_NEUTRAL_V2_RUNTIME_ROOT}/atlases/fantasy_neutral__world__way_segment__page003.png"
    "${FANTASY_NEUTRAL_V2_RUNTIME_ROOT}/atlases/fantasy_neutral__world__way_segment__page004.png"
    "${FANTASY_NEUTRAL_V2_RUNTIME_ROOT}/atlases/fantasy_neutral__world__way_segment__page006.png"
    "${FANTASY_NEUTRAL_V2_RUNTIME_ROOT}/atlases/fantasy_neutral__world__way_segment__page007.png"
    DESTINATION Resources/Maps/Tilesets/Generated/FantasyNeutral/v2/atlases)
function(install_manifested_fantasy_neutral_snapshot version_directory)
    set(snapshot_source_root
        "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Maps/Tilesets/Generated/FantasyNeutral/${version_directory}")
    set(pack_manifest "${snapshot_source_root}/pack_manifest.json")
    set(style_snapshot "${snapshot_source_root}/production_style_snapshot.json")

    foreach(required_metadata IN ITEMS "${pack_manifest}" "${style_snapshot}")
        if(NOT EXISTS "${required_metadata}")
            message(FATAL_ERROR
                "FantasyNeutral ${version_directory} production metadata is missing: ${required_metadata}")
        endif()
    endforeach()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${pack_manifest}"
        "${style_snapshot}")

    # CMake 3.14 has no JSON parser. Extract the deliberately simple relative
    # asset-file fields, then require the source directory to contain exactly
    # those files plus the two production metadata documents. This keeps CPack
    # from silently accepting local review or candidate files.
    file(READ "${pack_manifest}" manifest_json)
    string(REGEX MATCHALL
        "\"file\"[ \t\r\n]*:[ \t\r\n]*\"[^\"\r\n]+\""
        manifest_file_fields
        "${manifest_json}")
    if(NOT manifest_file_fields)
        message(FATAL_ERROR
            "FantasyNeutral ${version_directory} production manifest contains no asset files: ${pack_manifest}")
    endif()

    string(REGEX MATCHALL
        "\"status\"[ \t\r\n]*:[ \t\r\n]*\"accepted\""
        accepted_status_fields
        "${manifest_json}")
    list(LENGTH manifest_file_fields manifest_asset_count)
    list(LENGTH accepted_status_fields accepted_asset_count)
    if(NOT manifest_asset_count EQUAL accepted_asset_count)
        message(FATAL_ERROR
            "FantasyNeutral ${version_directory} production manifest is not an all-accepted asset set")
    endif()

    file(READ "${style_snapshot}" snapshot_json)
    if(NOT snapshot_json MATCHES "\"state\"[ \t\r\n]*:[ \t\r\n]*\"production\"")
        message(FATAL_ERROR
            "FantasyNeutral ${version_directory} style snapshot is not marked production: ${style_snapshot}")
    endif()

    set(snapshot_expected_files
        "pack_manifest.json"
        "production_style_snapshot.json")
    set(snapshot_asset_path_keys)
    foreach(manifest_file_field IN LISTS manifest_file_fields)
        string(REGEX REPLACE
            "^\"file\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"$"
            "\\1"
            asset_relative_path
            "${manifest_file_field}")
        string(REPLACE "\\" "/" asset_relative_path "${asset_relative_path}")
        if(asset_relative_path STREQUAL "" OR
           IS_ABSOLUTE "${asset_relative_path}" OR
           asset_relative_path MATCHES "(^|/)\\.\\.(/|$)")
            message(FATAL_ERROR
                "FantasyNeutral ${version_directory} manifest contains an unsafe asset path: ${asset_relative_path}")
        endif()

        string(TOLOWER "${asset_relative_path}" asset_path_key)
        list(FIND snapshot_asset_path_keys "${asset_path_key}" duplicate_asset_index)
        if(NOT duplicate_asset_index EQUAL -1)
            message(FATAL_ERROR
                "FantasyNeutral ${version_directory} manifest repeats asset path: ${asset_relative_path}")
        endif()
        list(APPEND snapshot_asset_path_keys "${asset_path_key}")

        set(asset_source "${snapshot_source_root}/${asset_relative_path}")
        if(NOT EXISTS "${asset_source}" OR IS_DIRECTORY "${asset_source}")
            message(FATAL_ERROR
                "FantasyNeutral ${version_directory} manifest asset is missing: ${asset_source}")
        endif()
        list(APPEND snapshot_expected_files "${asset_relative_path}")

        get_filename_component(asset_subdirectory "${asset_relative_path}" DIRECTORY)
        set(asset_destination
            "Resources/Maps/Tilesets/Generated/FantasyNeutral/${version_directory}")
        if(NOT asset_subdirectory STREQUAL "")
            string(APPEND asset_destination "/${asset_subdirectory}")
        endif()
        install(FILES "${asset_source}" DESTINATION "${asset_destination}")
    endforeach()

    file(GLOB_RECURSE snapshot_actual_files
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES false
        RELATIVE "${snapshot_source_root}"
        "${snapshot_source_root}/*")
    list(SORT snapshot_actual_files)
    list(SORT snapshot_expected_files)
    if(NOT "${snapshot_actual_files}" STREQUAL "${snapshot_expected_files}")
        message(FATAL_ERROR
            "FantasyNeutral ${version_directory} contains a file not represented by its production manifest, or is missing one")
    endif()

    install(FILES "${pack_manifest}" "${style_snapshot}"
        DESTINATION "Resources/Maps/Tilesets/Generated/FantasyNeutral/${version_directory}")
endfunction()

foreach(fantasy_neutral_snapshot IN ITEMS v3 v3_1 v3_2 v3_3)
    install_manifested_fantasy_neutral_snapshot("${fantasy_neutral_snapshot}")
endforeach()

set(CPACK_PACKAGE_NAME "GM Editor")
set(CPACK_PACKAGE_VENDOR "GM Editor")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "GM-Editor-${PROJECT_VERSION}-windows-x64")
set(CPACK_GENERATOR "ZIP")
include(CPack)
