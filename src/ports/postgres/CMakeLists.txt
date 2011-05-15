# ------------------------------------------------------------------------------
# Database Connector for PostgreSQL
# ------------------------------------------------------------------------------

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

message(STATUS "Trying to find PostgreSQL installation")
find_package(PostgreSQL)

if(POSTGRESQL_FOUND)
    message(STATUS "***")
    message(STATUS "*** Adding PostgreSQL to target list...")
    message(STATUS "***")

    include_directories(${POSTGRESQL_SERVER_INCLUDE_DIR})

    # Create library
    add_library(
        madlib_postgres
        MODULE
        ${MAD_SOURCES}
    )
    add_dependencies(madlib_postgres madlib)
    install(TARGETS madlib_postgres
        LIBRARY DESTINATION ports/postgres/lib
    )
    set_target_properties(madlib_postgres PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY lib
        BUILD_WITH_INSTALL_RPATH YES
    )

    target_link_libraries(madlib_postgres madlib)
    if(APPLE)
        osx_archs(${POSTGRESQL_EXECUTABLE} MAD_ARCHITECTURES)
        message(STATUS "Will build madlib PostgreSQL connector for same "
            "architectures as detected in ${POSTGRESQL_EXECUTABLE}, which has "
            "architectures ${MAD_ARCHITECTURES}")
        set_target_properties(madlib_postgres PROPERTIES
            OSX_ARCHITECTURES "${MAD_ARCHITECTURES}"
            LINK_FLAGS "-bundle_loader ${POSTGRESQL_EXECUTABLE} -Wl,-rpath -Wl,@loader_path/../../../lib")

        # the RPATH to be used when installing. This is set to "$ORIGIN/../../lib"
        # because the core MADlib library will reside in $MADLIB_ROOT/lib
        # and the Greenplum connector library in $MADLIB_ROOT/ports/greenplum/lib"
        set_target_properties(madlib_postgres
            PROPERTIES INSTALL_RPATH "@loader_path/../../../lib")
    else(APPLE)
        # the RPATH to be used when installing. This is set to "$ORIGIN/../../lib"
        # because the core MADlib library will reside in $MADLIB_ROOT/lib
        # and the PostgreSQL connector library in $MADLIB_ROOT/ports/postgres/lib"
        set_target_properties(madlib_postgres
            PROPERTIES INSTALL_RPATH "\$ORIGIN/../../../lib")
    endif(APPLE)
    
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
