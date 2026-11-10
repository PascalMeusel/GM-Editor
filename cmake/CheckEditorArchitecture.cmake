# Enforce dependency direction at the durable model boundary. Controllers and
# views may use models; models must remain independent of either UI layer.
if(NOT DEFINED GM_SOURCE_ROOT)
    message(FATAL_ERROR "GM_SOURCE_ROOT is required")
endif()

file(GLOB_RECURSE model_files LIST_DIRECTORIES FALSE
    "${GM_SOURCE_ROOT}/Models/*.cpp"
    "${GM_SOURCE_ROOT}/Models/*.hpp"
    "${GM_SOURCE_ROOT}/Models/*.h")

set(violations)
foreach(source_file IN LISTS model_files)
    file(STRINGS "${source_file}" forbidden_includes
        REGEX "^[ \t]*#[ \t]*include[ \t]*[<\"](Controllers|Views)/")
    foreach(include_line IN LISTS forbidden_includes)
        file(RELATIVE_PATH relative_file "${GM_SOURCE_ROOT}" "${source_file}")
        list(APPEND violations "${relative_file}: ${include_line}")
    endforeach()
endforeach()

if(violations)
    list(JOIN violations "\n  " formatted)
    message(FATAL_ERROR
        "Model layer depends on editor UI code:\n  ${formatted}")
endif()

list(LENGTH model_files checked_count)
message(STATUS
    "Editor dependency direction checked ${checked_count} model files")
