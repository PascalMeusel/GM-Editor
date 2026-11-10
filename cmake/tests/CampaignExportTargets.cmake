# Include before TestIsolation.cmake so standalone package tests inherit its
# disposable database/settings boundary in addition to their own fresh fixture.
if(TTRPG_BUILD_TESTS)
    add_executable(campaign_export_tests tools/campaign_export_tests.cpp)
    target_link_libraries(campaign_export_tests PRIVATE gm_core)
    add_test(NAME campaign_export COMMAND campaign_export_tests)
endif()
