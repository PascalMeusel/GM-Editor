if(NOT DEFINED GM_SOURCE_ROOT)
    message(FATAL_ERROR "GM_SOURCE_ROOT is required")
endif()

include("${GM_SOURCE_ROOT}/cmake/ApplicationSources.cmake")

file(GLOB_RECURSE _discovered_sources LIST_DIRECTORIES FALSE
    "${GM_SOURCE_ROOT}/Controllers/*.cpp"
    "${GM_SOURCE_ROOT}/Models/*.cpp"
    "${GM_SOURCE_ROOT}/Views/*.cpp"
)
file(GLOB _application_sources LIST_DIRECTORIES FALSE
    "${GM_SOURCE_ROOT}/Application*.cpp"
)
list(APPEND _discovered_sources ${_application_sources})
list(REMOVE_DUPLICATES _discovered_sources)
list(SORT _discovered_sources)

set(_declared_sources
    ${APPLICATION_SOURCES}
    ${CONTROLLER_SOURCES}
    ${MODEL_SOURCES}
    ${VIEW_SOURCES}
)
list(LENGTH _declared_sources _declared_count)
list(REMOVE_DUPLICATES _declared_sources)
list(LENGTH _declared_sources _unique_declared_count)
if(NOT _declared_count EQUAL _unique_declared_count)
    message(FATAL_ERROR
        "Production source manifest contains duplicate declarations "
        "(${_declared_count} entries, ${_unique_declared_count} unique paths)"
    )
endif()
list(SORT _declared_sources)

set(_missing_sources)
foreach(_source IN LISTS _discovered_sources)
    if(NOT _source IN_LIST _declared_sources)
        file(RELATIVE_PATH _relative "${GM_SOURCE_ROOT}" "${_source}")
        list(APPEND _missing_sources "${_relative}")
    endif()
endforeach()

set(_stale_sources)
foreach(_source IN LISTS _declared_sources)
    if(NOT EXISTS "${_source}")
        file(RELATIVE_PATH _relative "${GM_SOURCE_ROOT}" "${_source}")
        list(APPEND _stale_sources "${_relative}")
    endif()
endforeach()

if(_missing_sources OR _stale_sources)
    list(JOIN _missing_sources "\n  " _missing)
    list(JOIN _stale_sources "\n  " _stale)
    message(FATAL_ERROR
        "Production source manifest is incomplete.\n"
        "Missing declarations:\n  ${_missing}\n"
        "Declared paths that do not exist:\n  ${_stale}"
    )
endif()

list(LENGTH _discovered_sources _source_count)
message(STATUS "Production source manifest covers ${_source_count} implementation files")
