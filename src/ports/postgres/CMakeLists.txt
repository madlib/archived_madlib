# ------------------------------------------------------------------------------
# Database Connector for PostgreSQL
# ------------------------------------------------------------------------------


# -- 1. Find PostgreSQL installation. Do nothing if not found ------------------

message(STATUS "Trying to find PostgreSQL installation")
find_package(PostgreSQL)

# -- 2. If PostgreSQL is found, specify files that will be compiled into the
#       shared library ---------------------------------------------------------

if(POSTGRESQL_FOUND)
    message(STATUS "***")
    message(STATUS "*** Adding PostgreSQL to target list...")
    message(STATUS "***")

    set(MAD_DBAL_SOURCES_POSTGRES
        dbconnector/PGAbstractValue.cpp
        dbconnector/PGCompatibility.cpp
        dbconnector/PGNewDelete.cpp
        dbconnector/PGAllocator.cpp
        dbconnector/PGInterface.cpp
        dbconnector/PGMain.cpp
        dbconnector/PGToDatumConverter.cpp
        dbconnector/PGValue.cpp
    )
    
    # FIXME: Convert legacy source code written in C
    # BEGIN Legacy Code
    
        if(${CMAKE_COMPILER_IS_GNUCC})
            set(CMAKE_C_FLAGS "-std=c99")
        endif(${CMAKE_COMPILER_IS_GNUCC})
        file(GLOB_RECURSE LEGACY_C_FILES_POSTGRES RELATIVE
            "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/../methods/*.c")

        # errOmitLocation is used in legacy C code, but it is Greenplum only
        # we define it so that it has no effect for PostgreSQL.
        set_source_files_properties(${LEGACY_C_FILES_POSTGRES} PROPERTIES
            COMPILE_DEFINITIONS "NO_PG_MODULE_MAGIC;errOmitLocation=(int)"
        )
        list(APPEND MAD_DBAL_SOURCES_POSTGRES ${LEGACY_C_FILES_POSTGRES})
    
    # END Legacy Code

    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
    include_directories(${POSTGRESQL_SERVER_INCLUDE_DIR})

# -- 3. Build MADlib connector -------------------------------------------------

    add_madlib_connector_library(postgres "${POSTGRESQL_EXECUTABLE}"
        MADLIB_POSTGRES_TARGETNAME ${MAD_DBAL_SOURCES_POSTGRES}
    )
    target_link_libraries(${MADLIB_POSTGRES_TARGETNAME} madlib)

# -- 4. Install MADlib connector -----------------------------------------------

    install(TARGETS ${MADLIB_POSTGRES_TARGETNAME}
        LIBRARY DESTINATION ports/postgres/lib
    )

# -- 5. Preprocess/copy all Python/SQL files -----------------------------------
    
    add_python_files(
        PYTHON_TARGET_FILES_POSTGRES
        "modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
    )
    add_custom_target(pythonFiles_Postgres ALL DEPENDS ${PYTHON_TARGET_FILES_POSTGRES})
    
    add_sql_files(
        SQL_TARGET_FILES_POSTGRES
        "modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
    )
    add_custom_target(sqlFiles_Postgres ALL DEPENDS ${SQL_TARGET_FILES_POSTGRES})

# -- 7. Install all Python/SQL files -------------------------------------------

    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/modules
        DESTINATION ports/postgres
        REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
    )

    add_subdirectory(madpack)
else(POSTGRESQL_FOUND)
    message(STATUS "***")
    message(STATUS "*** No PostgreSQL installation found. Skipping.")
    message(STATUS "***")
endif(POSTGRESQL_FOUND)
