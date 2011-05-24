# ------------------------------------------------------------------------------
# Database Connector for Greenplum
# ------------------------------------------------------------------------------


# -- 1. Find Greenplum installation. Do nothing if not found -------------------

message(STATUS "Trying to find Greenplum installation")
find_package(Greenplum)


# -- 2. If Greenplum is found, specify files that will be compiled into the
#       shared library ---------------------------------------------------------

if(GREENPLUM_FOUND)
    message(STATUS "***")
    message(STATUS "*** Adding Greenplum to target list...")
    message(STATUS "***")

    set(MAD_DBAL_SOURCES_GREENPLUM
        ../postgres/dbconnector/PGAbstractValue.cpp
        dbconnector/GPCompatibility.cpp
        ../postgres/dbconnector/PGNewDelete.cpp
        ../postgres/dbconnector/PGAllocator.cpp
        ../postgres/dbconnector/PGInterface.cpp
        ../postgres/dbconnector/PGToDatumConverter.cpp
        ../postgres/dbconnector/PGValue.cpp
    )

    if(LINUX)
        list(APPEND MAD_DBAL_SOURCES_GREENPLUM
            linux/GPMain.cpp
        )
    else(LINUX)
        list(APPEND MAD_DBAL_SOURCES_GREENPLUM
            ../postgres/dbconnector/PGMain.cpp
        )
    endif(LINUX)

    # FIXME: Convert legacy source code written in C
    # BEGIN Legacy Code
    
        if(${CMAKE_COMPILER_IS_GNUCC})
            set(CMAKE_C_FLAGS "-std=c99")
        endif(${CMAKE_COMPILER_IS_GNUCC})
        file(GLOB_RECURSE LEGACY_C_FILES_GREENPLUM RELATIVE
            "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/../methods/*.c")

        set_source_files_properties(${LEGACY_C_FILES_GREENPLUM} PROPERTIES
            COMPILE_DEFINITIONS "NO_PG_MODULE_MAGIC"
        )
        list(APPEND MAD_DBAL_SOURCES_GREENPLUM ${LEGACY_C_FILES_GREENPLUM})
        # sparse_vector.c indirectly includes openssl/ssl.h (via libpq/libpq-be.h)
        include_directories(${GREENPLUM_BASE_DIR}/include)
    
    # END Legacy Code

    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/../postgres
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    include_directories(${GREENPLUM_INCLUDE_DIR})
    
    
# -- 3. Some versions of Greenplum ship with header files that need a patch to
#       be compatible with C++ -------------------------------------------------

    # Directory we will use for patches
    set(PATCH_DIR ${CMAKE_CURRENT_BINARY_DIR}/patch)
    
    # Version 4.1 of Greenplum has a bug in one of its header files that
    # needs to be fixed when building with C++.
    set(_GP_VERSION "${GREENPLUM_VERSION_MAJOR}.${GREENPLUM_VERSION_MINOR}.${GREENPLUM_VERSION_PATCH}")
    if(${_GP_VERSION} VERSION_EQUAL "4.1.0" OR ${_GP_VERSION} VERSION_EQUAL "4.1.1")
        
        message(STATUS "Installed Greenplum version has bug in header file. Applying patch...")
        
        # The first element in GREENPLUM_INCLUDE_DIR is include/server
        list(GET GREENPLUM_INCLUDE_DIR 0 GREENPLUM_SERVER_INCLUDE_DIR)
        
        file(COPY ${GREENPLUM_SERVER_INCLUDE_DIR}/utils/syncbitvector.h DESTINATION
            ${PATCH_DIR}/server/utils)
        execute_process(
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/patch/Versions_4_1_0_to_4_1_1.sh
            WORKING_DIRECTORY ${PATCH_DIR}
        )
        
        # Append directory with patched header file at beginning
        include_directories(BEFORE ${PATCH_DIR}/server)
    endif(${_GP_VERSION} VERSION_EQUAL "4.1.0" OR ${_GP_VERSION} VERSION_EQUAL "4.1.1")


# -- 4. Build MADlib connector -------------------------------------------------

    add_madlib_connector_library(greenplum "${GREENPLUM_EXECUTABLE}"
        MADLIB_GREENPLUM_TARGETNAME ${MAD_DBAL_SOURCES_GREENPLUM}
    )
    if(NOT LINUX)
        # On non-Linux platforms (where there is direct binding), we declare
        # madlib as a dependency. On Linux, madlib is laoded dynamically.
        target_link_libraries(${MADLIB_GREENPLUM_TARGETNAME} madlib)
    endif(NOT LINUX)


# -- 5. Install MADlib connector -----------------------------------------------

    install(TARGETS ${MADLIB_GREENPLUM_TARGETNAME}
        LIBRARY DESTINATION ports/greenplum/lib
    )
    

# -- 6. Preprocess/copy all Python/SQL files -----------------------------------

    set(M4_ARGUMENTS_GREENPLUM
        "-DGREENPLUM"
    )
    add_python_files(
        PYTHON_TARGET_FILES_GREENPLUM
        "../postgres/modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
        ${M4_ARGUMENTS_GREENPLUM}
    )
    # FIXME: Check legacy code for compliance with new architecture
    # BEGIN Legacy Code
        
        file(GLOB_RECURSE LEGACY_SQL_FILES_GREENPLUM RELATIVE
            "${CMAKE_SOURCE_DIR}/../methods"
            "${CMAKE_SOURCE_DIR}/../methods/*.py_in")
        foreach(CURRENT_FILE ${LEGACY_SQL_FILES_GREENPLUM})
            get_filename_component(CURRENT_PATH "${CMAKE_SOURCE_DIR}/../methods/${CURRENT_FILE}" ABSOLUTE)
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
            list(APPEND PYTHON_TARGET_FILES_GREENPLUM ${OUTFILE})
        endforeach(CURRENT_FILE ${LEGACY_SQL_FILES_GREENPLUM})
        
    # END Legacy Code
    add_custom_target(pythonFiles_Greenplum ALL DEPENDS ${PYTHON_TARGET_FILES_GREENPLUM})
    
    add_sql_files(
        SQL_TARGET_FILES_GREENPLUM
        "../postgres/modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
    )
    # FIXME: Check legacy code for compliance with new architecture
    # BEGIN Legacy Code
        
        file(GLOB_RECURSE LEGACY_SQL_FILES_GREENPLUM RELATIVE
            "${CMAKE_SOURCE_DIR}/../methods"
            "${CMAKE_SOURCE_DIR}/../methods/*.sql_in")
        foreach(CURRENT_FILE ${LEGACY_SQL_FILES_GREENPLUM})
            get_filename_component(CURRENT_PATH "${CMAKE_SOURCE_DIR}/../methods/${CURRENT_FILE}" ABSOLUTE)
            set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/modules/${CURRENT_FILE}")
            string(REPLACE "/src/pg_gp" "" OUTFILE ${OUTFILE})
            string(REPLACE "/sql/" "/test/" OUTFILE ${OUTFILE})
            add_custom_command(OUTPUT "${OUTFILE}"
                COMMAND ${CMAKE_COMMAND} -E copy "${CURRENT_PATH}" "${OUTFILE}"
                DEPENDS "${CURRENT_PATH}"
                COMMENT "Copying ${CURRENT_FILE}"
            )
            list(APPEND SQL_TARGET_FILES_GREENPLUM ${OUTFILE})
        endforeach(CURRENT_FILE ${LEGACY_SQL_FILES_GREENPLUM})
        
    # END Legacy Code
    add_custom_target(sqlFiles_Greenplum ALL DEPENDS ${SQL_TARGET_FILES_GREENPLUM})


# -- 7. Install all Python/SQL files -------------------------------------------

    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/modules
        DESTINATION ports/greenplum
        REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
    )

    add_subdirectory(madpack)
else(GREENPLUM_FOUND)
    message(STATUS "***")
    message(STATUS "*** No Greenplum installation found. Skipping.")
    message(STATUS "***")
endif(GREENPLUM_FOUND)
