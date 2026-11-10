# Isolate every registered test. Preserve producer/report dependencies and their
# explicitly configured working directories; database/preferences are per test.
if(TTRPG_BUILD_TESTS)
    add_test(NAME ci_merge_gate_tests COMMAND powershell -NoProfile -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/ci/test-merge-gate.ps1")
    # Run these exhaustive workloads in every Release checkpoint. Focused
    # Debug keeps the remaining behavioral regressions; manual full Debug
    # includes these unchanged tests and assertions as well.
    set_tests_properties(tactical_v10_tests tactical_v11_tests tactical_v12_tests
        travel_scene_tests map_trace_export_tests
        map_generation_release_qa_smoke map_generation_release_qa_v5_contract
        PROPERTIES LABELS "extended-debug")
    get_property(ttrpg_tests DIRECTORY PROPERTY TESTS)
    set(ttrpg_test_root_ids)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/test-data")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/test-data/test-roots.tsv" "test\tdirectory\n")
    foreach(test_name IN LISTS ttrpg_tests)
        # Focused Debug must be closed over producer/report dependencies.
        # A DEPENDS edge orders selected tests; CTest does not select a missing
        # producer automatically when --label-exclude removes it.
        get_property(profile_labels TEST "${test_name}" PROPERTY LABELS)
        if(NOT "extended-debug" IN_LIST profile_labels)
            get_property(profile_dependencies TEST "${test_name}" PROPERTY DEPENDS)
            foreach(producer IN LISTS profile_dependencies)
                get_property(producer_labels TEST "${producer}" PROPERTY LABELS)
                if("extended-debug" IN_LIST producer_labels)
                    message(FATAL_ERROR "Focused Debug test ${test_name} requires excluded producer ${producer}")
                endif()
            endforeach()
        endif()
        # Recovery fixtures create nested campaign/session paths. Long test names
        # can exhaust the Windows directory limit even in an ordinary checkout.
        # Keep deterministic, isolated short roots and a readable evidence index.
        string(SHA256 test_root_digest "${test_name}")
        string(SUBSTRING "${test_root_digest}" 0 16 test_root_id)
        if(test_root_id IN_LIST ttrpg_test_root_ids)
            message(FATAL_ERROR "Duplicate test isolation directory for ${test_name}")
        endif()
        list(APPEND ttrpg_test_root_ids "${test_root_id}")
        set(test_root "${CMAKE_CURRENT_BINARY_DIR}/test-data/${test_root_id}")
        file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/test-data/test-roots.tsv" "${test_name}\t${test_root_id}\n")
        file(MAKE_DIRECTORY "${test_root}")
        get_property(test_workdir TEST "${test_name}" PROPERTY WORKING_DIRECTORY)
        if(NOT test_workdir)
            set_tests_properties("${test_name}" PROPERTIES WORKING_DIRECTORY "${test_root}")
        endif()
        get_property(test_environment TEST "${test_name}" PROPERTY ENVIRONMENT)
        list(FILTER test_environment EXCLUDE REGEX "^TTRPG_(DB_PATH|SETTINGS_PATH|DATA_ROOT)=")
        list(APPEND test_environment "TTRPG_DATA_ROOT=" "TTRPG_DB_PATH=${test_root}/TTRPG.db"
            "TTRPG_SETTINGS_PATH=${test_root}/GMEditor.settings")
        set_tests_properties("${test_name}" PROPERTIES ENVIRONMENT "${test_environment}")
        get_property(test_timeout TEST "${test_name}" PROPERTY TIMEOUT)
        if(NOT test_timeout)
            set(test_timeout 600)
        endif()
        # Unoptimized generator regressions spend many minutes in the checked
        # STL on MSVC. Preserve every case and assertion in Debug, with a longer
        # watchdog; performance acceptance is measured separately in Release.
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            math(EXPR test_timeout "${test_timeout} * 3")
        endif()
        set_tests_properties("${test_name}" PROPERTIES TIMEOUT "${test_timeout}")
    endforeach()
endif()
