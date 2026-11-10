if(NOT DEFINED GM_SOURCE_ROOT)
    message(FATAL_ERROR "GM_SOURCE_ROOT is required")
endif()

if(NOT DEFINED GM_MAX_LINES)
    set(GM_MAX_LINES 400)
endif()

# Discover every first-party production file recursively. This prevents a new
# oversized implementation from bypassing the contract by living outside a
# hand-maintained allow-list.
file(GLOB_RECURSE _production_files LIST_DIRECTORIES FALSE
    "${GM_SOURCE_ROOT}/Controllers/*.cpp"
    "${GM_SOURCE_ROOT}/Controllers/*.hpp"
    "${GM_SOURCE_ROOT}/Controllers/*.h"
    "${GM_SOURCE_ROOT}/Models/*.cpp"
    "${GM_SOURCE_ROOT}/Models/*.hpp"
    "${GM_SOURCE_ROOT}/Models/*.h"
    "${GM_SOURCE_ROOT}/Views/*.cpp"
    "${GM_SOURCE_ROOT}/Views/*.hpp"
    "${GM_SOURCE_ROOT}/Views/*.h"
)
file(GLOB _application_files LIST_DIRECTORIES FALSE
    "${GM_SOURCE_ROOT}/Application*.cpp"
    "${GM_SOURCE_ROOT}/Application*.hpp"
    "${GM_SOURCE_ROOT}/Application*.h"
    "${GM_SOURCE_ROOT}/main.cpp"
)
file(GLOB_RECURSE _build_modules LIST_DIRECTORIES FALSE
    "${GM_SOURCE_ROOT}/cmake/*.cmake")

set(_files ${_production_files} ${_application_files}
    "${GM_SOURCE_ROOT}/CMakeLists.txt" ${_build_modules})
list(REMOVE_DUPLICATES _files)
list(SORT _files)

set(_violations)
foreach(_file IN LISTS _files)
    file(READ "${_file}" _contents)
    string(REGEX REPLACE "[^\n]" "" _newlines "${_contents}")
    string(LENGTH "${_newlines}" _line_count)
    if(NOT _contents STREQUAL "" AND NOT _contents MATCHES "\n$")
        math(EXPR _line_count "${_line_count} + 1")
    endif()
    if(_line_count GREATER GM_MAX_LINES)
        file(RELATIVE_PATH _relative "${GM_SOURCE_ROOT}" "${_file}")
        list(APPEND _violations "${_relative} (${_line_count} lines)")
    endif()
endforeach()

# Controllers coordinate multiple services by nature, but their public/private
# façade should stay small enough that cohesive state and workflows live in
# dedicated collaborators instead of accumulating in another god object.
set(GM_CONTROLLER_FACADE_MAX_LINES 250)
file(GLOB_RECURSE _controller_facades LIST_DIRECTORIES FALSE
    "${GM_SOURCE_ROOT}/Controllers/*Controller.hpp")
foreach(_file IN LISTS _controller_facades)
    file(READ "${_file}" _controller_contents)
    string(REGEX REPLACE "[^\n]" "" _controller_newlines "${_controller_contents}")
    string(LENGTH "${_controller_newlines}" _controller_line_count)
    if(NOT _controller_contents STREQUAL "" AND NOT _controller_contents MATCHES "\n$")
        math(EXPR _controller_line_count "${_controller_line_count} + 1")
    endif()
    if(_controller_line_count GREATER GM_CONTROLLER_FACADE_MAX_LINES)
        file(RELATIVE_PATH _relative "${GM_SOURCE_ROOT}" "${_file}")
        list(APPEND _violations
            "${_relative} (${_controller_line_count} lines; controller façade limit is ${GM_CONTROLLER_FACADE_MAX_LINES})")
    endif()
endforeach()

list(LENGTH _files _checked_count)
if(_violations)
    list(JOIN _violations "\n  " _formatted)
    message(FATAL_ERROR
        "Production modules exceed the ${GM_MAX_LINES}-line limit:\n  ${_formatted}"
    )
endif()

message(STATUS "Module-size contract checked ${_checked_count} production files (maximum ${GM_MAX_LINES} lines)")
