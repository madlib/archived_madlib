# ------------------------------------------------------------------------------
# madpack source files for PostgreSQL
# ------------------------------------------------------------------------------


# -- 1. Third-party dependencies: Download and build PyGreSQL ------------------

# join_strings(_CMAKE_MODULE_PATH
#     ${EXTERNALPROJECT_LIST_SEPARATOR} "${CMAKE_MODULE_PATH}")
# 
# join_strings(_CMAKE_PREFIX_PATH
#     ${EXTERNALPROJECT_LIST_SEPARATOR} "${CMAKE_PREFIX_PATH}")
# 
# ExternalProject_Add(EP_pygresql_postgresql
#     PREFIX ${MAD_THIRD_PARTY}
#     LIST_SEPARATOR ${EXTERNALPROJECT_LIST_SEPARATOR}
#     DOWNLOAD_DIR ${MAD_THIRD_PARTY}/downloads
#     URL ${PYGRESQL_TAR_SOURCE}
#     URL_MD5 ${PYGRESQL_TAR_MD5}
#     PATCH_COMMAND ${CMAKE_COMMAND} -E copy
#         ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists_PyGreSQL.txt
#         ${MAD_THIRD_PARTY}/src/EP_pygresql_postgresql/CMakeLists.txt
#     CMAKE_ARGS
#         "-DCMAKE_MODULE_PATH=${_CMAKE_MODULE_PATH}"
#         "-DCMAKE_PREFIX_PATH=${_CMAKE_PREFIX_PATH}"
#         "-DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}"
#         "-DPOSTGRESQL_CLIENT_INCLUDE_DIR=${POSTGRESQL_CLIENT_INCLUDE_DIR}"
#         "-DPOSTGRESQL_SERVER_INCLUDE_DIR=${POSTGRESQL_SERVER_INCLUDE_DIR}"
#         "-DPOSTGRESQL_LIB_DIR=${POSTGRESQL_LIB_DIR}"
#     BINARY_DIR ${MAD_THIRD_PARTY}/src/EP_pygresql_postgresql
# )

# -- 2. Copy all *.py and *.m4 files -------------------------------------------

file(GLOB_RECURSE MADPACK_FILES_POSTGRES
    RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "*.py"
    "*.m4"
)
add_files(MADPACK_TARGET_FILES_POSTGRES . "${CMAKE_CURRENT_BINARY_DIR}" ${MADPACK_FILES_POSTGRES})
add_custom_target(madpackFiles_Postgres ALL DEPENDS ${MADPACK_TARGET_FILES_POSTGRES})


# -- 3. Install madpack files to $MADLIB_ROOT/ports/postgres/madpack ----------

# install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pygresql
#     DESTINATION ports/postgres/madpack
#     COMPONENT postgres
#     REGEX ".*/\\.DS_Store\$" EXCLUDE
# )

install(FILES ${MADPACK_TARGET_FILES_POSTGRES}
    DESTINATION ports/postgres/madpack
    COMPONENT postgres
)
