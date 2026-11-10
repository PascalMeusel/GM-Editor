# Explicit application inputs. Add new production translation units here.
get_filename_component(GM_APPLICATION_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

include("${GM_APPLICATION_SOURCE_ROOT}/cmake/sources/Application.cmake")
include("${GM_APPLICATION_SOURCE_ROOT}/cmake/sources/Controllers.cmake")
include("${GM_APPLICATION_SOURCE_ROOT}/cmake/sources/Views.cmake")
include("${GM_APPLICATION_SOURCE_ROOT}/cmake/sources/Models.cmake")
