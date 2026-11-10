add_executable(seed_cyberpunk_campaign
    tools/seed_cyberpunk_campaign.cpp
)

add_executable(seed_current_feature_tour
    tools/seed_current_feature_tour.cpp
)

add_executable(seed_release_demo_de
    tools/seed_release_demo_de.cpp
)

# The campaign seeders need the persistence/domain model, but never render or
# export maps.  Keep the optional PNG exporter out of these command-line tools;
# it depends on the editor's platform image loader (and GDI+/libpng).
set(SEED_MODEL_SOURCES ${MODEL_SOURCES})
list(REMOVE_ITEM SEED_MODEL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Models/Map/MapPngExport.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Models/Map/MapImageImportService.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Models/Map/MapImageImportInternal.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Models/Map/MapImageImportTrace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Models/Map/MapImageImportLegacyReference.cpp"
)

add_executable(seed_blackwater_campaign
    tools/seed_blackwater_campaign.cpp
    ${SEED_MODEL_SOURCES}
)

add_executable(seed_clockwork_bloodline
    tools/seed_clockwork_bloodline.cpp
    ${SEED_MODEL_SOURCES}
)

add_executable(seed_onboarding_sample
    tools/seed_onboarding_sample.cpp
)

configure_ttrpg_sqlite_target(seed_cyberpunk_campaign)
configure_ttrpg_sqlite_target(seed_current_feature_tour)
configure_ttrpg_sqlite_target(seed_release_demo_de)
configure_ttrpg_sqlite_target(seed_blackwater_campaign)
configure_ttrpg_sqlite_target(seed_clockwork_bloodline)
configure_ttrpg_sqlite_target(seed_onboarding_sample)

option(TTRPG_BUILD_TESTS "Build TTRPG editor tests" ON)
