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
            COMMAND ${MAD_BUILD_TOOLS}/greenplum_patch_4_1_x.sh
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

    list(APPEND M4_ARGUMENTS_GREENPLUM
        "-DGREENPLUM"
    )
    add_python_files(
        PYTHON_TARGET_FILES_GREENPLUM
        "../postgres/modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
        ${M4_ARGUMENTS_GREENPLUM}
    )
    add_custom_target(pythonFiles_Greenplum ALL DEPENDS ${PYTHON_TARGET_FILES_GREENPLUM})
    
    add_sql_files(
        SQL_TARGET_FILES_GREENPLUM
        "../postgres/modules"
        "${CMAKE_CURRENT_BINARY_DIR}/modules"
    )
    add_custom_target(sqlFiles_Greenplum ALL DEPENDS ${SQL_TARGET_FILES_GREENPLUM})


# -- 7. Install all Python/SQL files -------------------------------------------

    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/modules
        DESTINATION ports/greenplum
        REGEX "^(.*/)?\\.DS_Store\$" EXCLUDE
    )
    
else(GREENPLUM_FOUND)
    message(STATUS "***")
    message(STATUS "*** No Greenplum installation found. Skipping.")
    message(STATUS "***")
endif(GREENPLUM_FOUND)
