# Set defaults that can be overridden by files that include this file:
if(NOT DEFINED _FIND_PACKAGE_FILE)
    set(_FIND_PACKAGE_FILE "${CMAKE_CURRENT_LIST_FILE}")
endif(NOT DEFINED _FIND_PACKAGE_FILE)

# Set parameters for calling FindPostgreSQL.cmake
set(_NEEDED_PG_CONFIG_PACKAGE_NAME "Greenplum Database")
set(_PG_CONFIG_VERSION_NUM_MACRO "GP_VERSION_NUM")
set(_PG_CONFIG_VERSION_MACRO "GP_VERSION")
set(_SEARCH_PATH_HINTS
    "/usr/local/greenplum-db/bin"
    "$ENV{GPHOME}/bin"
)

include("${CMAKE_CURRENT_LIST_DIR}/../../postgres/cmake/FindPostgreSQL.cmake")

if(${PKG_NAME}_FOUND)
    # server/funcapi.h ultimately includes server/access/xact.h, from which
    # cdb/cdbpathlocus.h is included
    execute_process(COMMAND ${${PKG_NAME}_PG_CONFIG} --pkgincludedir
        OUTPUT_VARIABLE ${PKG_NAME}_ADDITIONAL_INCLUDE_DIRS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(${PKG_NAME}_ADDITIONAL_INCLUDE_DIRS
        "${${PKG_NAME}_ADDITIONAL_INCLUDE_DIRS}/internal")
endif(${PKG_NAME}_FOUND)
