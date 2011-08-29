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

    add_executable(greenplum IMPORTED)
    set_target_properties(greenplum PROPERTIES
        IMPORTED_LOCATION "${GREENPLUM_EXECUTABLE}"
    )

    set(MAD_DBAL_SOURCES_GREENPLUM
        ../postgres/dbconnector/PGAbstractValue.cpp
        ../postgres/dbconnector/PGAbstractValue.hpp
        ../postgres/dbconnector/PGAllocator.cpp
        ../postgres/dbconnector/PGAllocator.hpp
        ../postgres/dbconnector/PGArrayHandle.cpp
        ../postgres/dbconnector/PGArrayHandle.hpp
        ../postgres/dbconnector/PGCommon.hpp
        dbconnector/GPCompatibility.cpp
        ../postgres/dbconnector/PGCompatibility.hpp
        ../postgres/dbconnector/PGInterface.cpp
        ../postgres/dbconnector/PGInterface.hpp
        ../postgres/dbconnector/PGMain.cpp
        ../postgres/dbconnector/PGMain.hpp
        ../postgres/dbconnector/PGNewDelete.cpp
        ../postgres/dbconnector/PGToDatumConverter.cpp
        ../postgres/dbconnector/PGToDatumConverter.hpp
        ../postgres/dbconnector/PGValue.cpp
        ../postgres/dbconnector/PGValue.hpp
    )
    if(LINUX)
        list(APPEND MAD_DBAL_SOURCES_GREENPLUM
            linux/dbconnector/GPArmadilloGlue.cpp
        )
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../linux)
    endif(LINUX)

    # FIXME: Convert legacy source code written in C
    # BEGIN Legacy Code
    
        if(${CMAKE_COMPILER_IS_GNUCC})
            # Due to Greenplum bug MPP-13254, we need to include <sys/time.h>
            # before <postgres.h>
            set(CMAKE_C_FLAGS "-std=gnu99 -include sys/time.h")
        endif(${CMAKE_COMPILER_IS_GNUCC})
        file(GLOB_RECURSE LEGACY_C_FILES_GREENPLUM RELATIVE
            "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_SOURCE_DIR}/methods/*.c")

        set_source_files_properties(${LEGACY_C_FILES_GREENPLUM} PROPERTIES
            COMPILE_DEFINITIONS "NO_PG_MODULE_MAGIC"
        )
        list(APPEND MAD_DBAL_SOURCES_GREENPLUM ${LEGACY_C_FILES_GREENPLUM})
        # sparse_vector.c indirectly includes openssl/ssl.h (via libpq/libpq-be.h)
        include_directories(${GREENPLUM_CLIENT_INCLUDE_DIR})
    
    # END Legacy Code

    include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}/../postgres
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    include_directories(${GREENPLUM_SERVER_INCLUDE_DIR})
    include_directories(${GREENPLUM_ADDITIONAL_INCLUDE_DIRS})


# -- 3. Build MADlib connector -------------------------------------------------

    add_madlib_connector_library(greenplum "${GREENPLUM_EXECUTABLE}"
        MADLIB_GREENPLUM_TARGETNAME ${MAD_DBAL_SOURCES_GREENPLUM}
    )
    target_link_libraries(${MADLIB_GREENPLUM_TARGETNAME} madlib)

    
# -- 4. Some versions of Greenplum ship with header files that need a patch to
#       be compatible with C++ -------------------------------------------------

    # Directory we will use for patches
    set(PATCH_DIR ${CMAKE_CURRENT_BINARY_DIR}/patch)
    set(_GP_VERSION "${GREENPLUM_VERSION_MAJOR}.${GREENPLUM_VERSION_MINOR}.${GREENPLUM_VERSION_PATCH}")

    # Version 4.0 of Greenplum and below ship with a utils/builtin.h that has
    # C++ keywords as identifiers. We need to patch this.
    if(${_GP_VERSION} VERSION_LESS "4.1")
        message(STATUS "Installed Greenplum version has bug in header file. "
            "Will apply patch for versions below 4.1.")
        
        add_custom_command(
            OUTPUT
                "${PATCH_DIR}/server/utils/builtins.h"
            COMMAND 
                "${CMAKE_CURRENT_BINARY_DIR}/patch/Versions_4_0_and_below.sh"
            DEPENDS
                "${GREENPLUM_SERVER_INCLUDE_DIR}/utils/builtins.h"
                "${CMAKE_CURRENT_BINARY_DIR}/patch/Versions_4_0_and_below.sh"
            WORKING_DIRECTORY "${PATCH_DIR}"
            COMMENT "Patching builtins.h: Renaming identifiers that are C++ keywords."
        )
        list(APPEND GREENPLUM_PATCH_FILES
            "${PATCH_DIR}/server/utils/builtins.h"
        )
    endif(${_GP_VERSION} VERSION_LESS "4.1")
    
    # Version 4.1 of Greenplum has a bug in one of its header files that
    # needs to be fixed when building with C++.
    if(${_GP_VERSION} VERSION_EQUAL "4.1.0" OR ${_GP_VERSION} VERSION_EQUAL "4.1.1")
        message(STATUS "Installed Greenplum version has bug in header file. "
            "Will apply patch for versions 4.1.0 to 4.1.1.")
        
        add_custom_command(
            OUTPUT
                "${PATCH_DIR}/server/utils/syncbitvector.h"
            COMMAND
                "${CMAKE_CURRENT_BINARY_DIR}/patch/Versions_4_1_0_to_4_1_1.sh"
            DEPENDS
                "${GREENPLUM_SERVER_INCLUDE_DIR}/utils/syncbitvector.h"
                "${CMAKE_CURRENT_BINARY_DIR}/patch/Versions_4_1_0_to_4_1_1.sh"
            WORKING_DIRECTORY "${PATCH_DIR}"
            COMMENT "Patching syncbitvector.h: Add missing key word \"typedef\"."
        )
        list(APPEND GREENPLUM_PATCH_FILES
            "${PATCH_DIR}/server/utils/syncbitvector.h"
        )
    endif(${_GP_VERSION} VERSION_EQUAL "4.1.0" OR ${_GP_VERSION} VERSION_EQUAL "4.1.1")

    if(GREENPLUM_PATCH_FILES)
        add_subdirectory(patch)
        add_custom_target(greenplumPatches ALL DEPENDS ${GREENPLUM_PATCH_FILES})
        # Append directory with patched header file at beginning
        include_directories(BEFORE ${PATCH_DIR}/server)
        add_dependencies(madlib_greenplum greenplumPatches)
    endif(GREENPLUM_PATCH_FILES)
        

# -- 5. Install MADlib connector -----------------------------------------------

    install(TARGETS ${MADLIB_GREENPLUM_TARGETNAME}
        LIBRARY DESTINATION ports/greenplum/lib
        COMPONENT greenplum
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
            "${CMAKE_SOURCE_DIR}/methods"
            "${CMAKE_SOURCE_DIR}/methods/*.py_in")
        foreach(CURRENT_FILE ${LEGACY_SQL_FILES_GREENPLUM})
            get_filename_component(CURRENT_PATH "${CMAKE_SOURCE_DIR}/methods/${CURRENT_FILE}" ABSOLUTE)
            set(OUTFILE "${CMAKE_CURRENT_BINARY_DIR}/modules/${CURRENT_FILE}")
            string(REPLACE "/src/pg_gp" "" OUTFILE ${OUTFILE})
            string(REGEX REPLACE ".py_in\$" ".py" OUTFILE ${OUTFILE})
            get_dir_name(OUTDIR ${OUTFILE})
            add_custom_command(OUTPUT "${OUTFILE}"
                COMMAND ${CMAKE_COMMAND} -E make_directory "${OUTDIR}"
                COMMAND ${M4_BINARY} ${M4_ARGUMENTS} ${M4_ARGUMENTS_GREENPLUM} "${CURRENT_PATH}" > "${OUTFILE}"
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
    # Add Greenplum-specific modules. Files will be appended to
    # SQL_TARGET_FILES_GREENPLUM.
    add_sql_files(
        SQL_TARGET_FILES_GREENPLUM
        "modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
    )
    # FIXME: Check legacy code for compliance with new architecture
    # BEGIN Legacy Code
        
        file(GLOB_RECURSE LEGACY_SQL_FILES_GREENPLUM RELATIVE
            "${CMAKE_SOURCE_DIR}/methods"
            "${CMAKE_SOURCE_DIR}/methods/*.sql_in")
        foreach(CURRENT_FILE ${LEGACY_SQL_FILES_GREENPLUM})
            get_filename_component(CURRENT_PATH "${CMAKE_SOURCE_DIR}/methods/${CURRENT_FILE}" ABSOLUTE)
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
        COMPONENT greenplum
        REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
    )

    add_subdirectory(madpack)
else(GREENPLUM_FOUND)
    message(STATUS "***")
    message(STATUS "*** No Greenplum installation found. Skipping.")
    message(STATUS "***")
endif(GREENPLUM_FOUND)
