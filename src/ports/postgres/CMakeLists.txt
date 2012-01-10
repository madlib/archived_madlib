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

    add_library(postgres STATIC IMPORTED)
    set_target_properties(postgres PROPERTIES
        IMPORTED_LOCATION "${POSTGRESQL_EXECUTABLE}"
    )

    set(MAD_DBAL_SOURCES_POSTGRES
        dbconnector/PGAbstractType.cpp
        dbconnector/PGAbstractType.hpp
        dbconnector/PGAllocator.cpp
        dbconnector/PGAllocator.hpp
        dbconnector/PGArrayHandle.cpp
        dbconnector/PGArrayHandle.hpp
        dbconnector/PGCommon.hpp
        dbconnector/PGCompatibility.cpp
        dbconnector/PGCompatibility.hpp
        dbconnector/PGInterface.cpp
        dbconnector/PGInterface.hpp
        dbconnector/PGMain.cpp
        dbconnector/PGNewDelete.cpp
        dbconnector/PGToDatumConverter.cpp
        dbconnector/PGToDatumConverter.hpp
        dbconnector/PGType.cpp
        dbconnector/PGType.hpp
    )
    if(LINUX)
        list(APPEND MAD_DBAL_SOURCES_POSTGRES
            linux/dbconnector/PGArmadilloGlue.cpp
        )
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../linux)
    endif(LINUX)
    
    # FIXME: Convert legacy source code written in C
    # BEGIN Legacy Code
    
        if(CMAKE_COMPILER_IS_GNUCC)
            # FIXME: In the C code, we have several places where strict aliasing
            # rules are violated. See this web page for some background:
            # http://dbp-consulting.com/tutorials/StrictAliasing.html
            # For now, we tell GCC that it cannot rely on strict aliasing rules.
            # Downside: We forgo some potential optimization.
            # The warning GCC would output without -fno-strict-aliasing is:
            # dereferencing type-punned pointer will break strict-aliasing rules
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-strict-aliasing")
        endif(CMAKE_COMPILER_IS_GNUCC)
        file(GLOB_RECURSE LEGACY_C_FILES_POSTGRES RELATIVE
            "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/methods/*.c")

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
    if(LINUX)
        # On Linux, we declare Armadillo a dependency of the connector not the
        # core library. See main CMakeLists.txt file why.
        target_link_libraries(${MADLIB_POSTGRES_TARGETNAME} armadillo)
    endif(LINUX)


# -- 4. Old versions of PostgreSQL ship with header files that need a patch to
#       be compatible with C++ -------------------------------------------------

    # Directory we will use for patches
    set(PATCH_DIR ${CMAKE_CURRENT_BINARY_DIR}/patch)
    set(_PG_VERSION "${POSTGRESQL_VERSION_MAJOR}.${POSTGRESQL_VERSION_MINOR}.${POSTGRESQL_VERSION_PATCH}")

    # Version 8.4 of PostgreSQL and below ship with server/nodes/parsenodes.h
    # and server/nodes/primnodes.h that have C++ keywords as identifiers.
    # We need to patch this.
    if(${_PG_VERSION} VERSION_LESS "9.0")
        message(STATUS "Installed PostgreSQL version has header files that need a patch for C++ compatibility. Will apply patch for versions below 9.0.")
        
        add_custom_command(
            OUTPUT
                "${PATCH_DIR}/server/nodes/parsenodes.h"
                "${PATCH_DIR}/server/nodes/primnodes.h"
                "${PATCH_DIR}/server/utils/builtins.h"
            COMMAND
                "${CMAKE_CURRENT_BINARY_DIR}/patch/Versions_8_4_and_below.sh"
            DEPENDS
                "${POSTGRESQL_SERVER_INCLUDE_DIR}/nodes/parsenodes.h"
                "${POSTGRESQL_SERVER_INCLUDE_DIR}/nodes/primnodes.h"
                "${POSTGRESQL_SERVER_INCLUDE_DIR}/utils/builtins.h"
                "${CMAKE_CURRENT_BINARY_DIR}/patch/Versions_8_4_and_below.sh"
            WORKING_DIRECTORY "${PATCH_DIR}"
            COMMENT "Patching parsenodes.h, primnodes.h, builtins.h, memnodes.h: Renaming identifiers that are C++ keywords."
        )
        list(APPEND POSTGRESQL_PATCH_FILES
            "${PATCH_DIR}/server/nodes/parsenodes.h"
            "${PATCH_DIR}/server/nodes/primnodes.h"
            "${PATCH_DIR}/server/utils/builtins.h"
        )
    endif(${_PG_VERSION} VERSION_LESS "9.0")

    if(POSTGRESQL_PATCH_FILES)
        add_subdirectory(patch)
        add_custom_target(postgresPatches ALL DEPENDS ${POSTGRESQL_PATCH_FILES})
        # Append directory with patched header file at beginning
        include_directories(BEFORE ${PATCH_DIR}/server)
        add_dependencies(madlib_postgres postgresPatches)
    endif(POSTGRESQL_PATCH_FILES)


# -- 5. Install MADlib connector -----------------------------------------------

    install(TARGETS ${MADLIB_POSTGRES_TARGETNAME}
        LIBRARY DESTINATION ports/postgres/lib
        COMPONENT postgres
    )

# -- 6. Preprocess/copy all Python/SQL files -----------------------------------

    # Make sure we support older Postgres versions while still using the
    # latest features of the newer ones
    if(${_PG_VERSION} VERSION_LESS "9.0")
        set(M4_ARGUMENTS_POSTGRES
            "-DPOSTGRES_PRE_90"
        )
    endif(${_PG_VERSION} VERSION_LESS "9.0")

    add_python_files(
        PYTHON_TARGET_FILES_POSTGRES
        "modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
        ${M4_ARGUMENTS_POSTGRES}
    )
    # FIXME: Check legacy code for compliance with new architecture
    # BEGIN Legacy Code
        
        file(GLOB_RECURSE LEGACY_SQL_FILES_POSTGRES RELATIVE
            "${CMAKE_SOURCE_DIR}/methods"
            "${CMAKE_SOURCE_DIR}/methods/*.py_in")
        foreach(CURRENT_FILE ${LEGACY_SQL_FILES_POSTGRES})
            get_filename_component(CURRENT_PATH "${CMAKE_SOURCE_DIR}/methods/${CURRENT_FILE}" ABSOLUTE)
            set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/modules/${CURRENT_FILE}")
            string(REPLACE "/src/pg_gp" "" OUTFILE ${OUTFILE})
            string(REGEX REPLACE ".py_in\$" ".py" OUTFILE ${OUTFILE})
            get_dir_name(OUTDIR ${OUTFILE})
            add_custom_command(OUTPUT "${OUTFILE}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTDIR}"
                COMMAND ${M4_BINARY} ${M4_ARGUMENTS} "${CURRENT_PATH}" > "${OUTFILE}"
                DEPENDS "${CURRENT_PATH}"
                COMMENT "Preprocessing ${CURRENT_FILE} with m4."
            )
            list(APPEND PYTHON_TARGET_FILES_POSTGRES ${OUTFILE})
        endforeach(CURRENT_FILE ${LEGACY_SQL_FILES_POSTGRES})
        
    # END Legacy Code
    add_custom_target(pythonFiles_Postgres ALL DEPENDS ${PYTHON_TARGET_FILES_POSTGRES})
    
    add_sql_files(
        SQL_TARGET_FILES_POSTGRES
        "modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
    )
    # FIXME: Check legacy code for compliance with new architecture
    # BEGIN Legacy Code
        
        file(GLOB_RECURSE LEGACY_SQL_FILES_POSTGRES RELATIVE
            "${CMAKE_SOURCE_DIR}/methods"
            "${CMAKE_SOURCE_DIR}/methods/*.sql_in")
        foreach(CURRENT_FILE ${LEGACY_SQL_FILES_POSTGRES})
            get_filename_component(CURRENT_PATH "${CMAKE_SOURCE_DIR}/methods/${CURRENT_FILE}" ABSOLUTE)
            set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/modules/${CURRENT_FILE}")
            string(REPLACE "/src/pg_gp" "" OUTFILE ${OUTFILE})
            string(REPLACE "/sql/" "/test/" OUTFILE ${OUTFILE})
            add_custom_command(OUTPUT "${OUTFILE}"
                COMMAND ${CMAKE_COMMAND} -E copy "${CURRENT_PATH}" "${OUTFILE}"
                DEPENDS "${CURRENT_PATH}"
                COMMENT "Copying ${CURRENT_FILE}"
            )
            list(APPEND SQL_TARGET_FILES_POSTGRES ${OUTFILE})
        endforeach(CURRENT_FILE ${LEGACY_SQL_FILES_POSTGRES})
        
    # END Legacy Code
    add_custom_target(sqlFiles_Postgres ALL DEPENDS ${SQL_TARGET_FILES_POSTGRES})
    
# -- 7. Install all Python/SQL files -------------------------------------------

    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/modules
        DESTINATION ports/postgres
        COMPONENT postgres
        REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
    )

    add_subdirectory(madpack)
else(POSTGRESQL_FOUND)
    message(STATUS "***")
    message(STATUS "*** No PostgreSQL installation found. Skipping.")
    message(STATUS "***")
endif(POSTGRESQL_FOUND)
