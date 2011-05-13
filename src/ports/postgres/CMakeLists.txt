set(MAD_DBAL_PG_SOURCES
	dbconnector/PGAbstractValue.cpp
	dbconnector/PGCompatibility.cpp
	dbconnector/PGNewDelete.cpp
	dbconnector/PGAllocator.cpp
	dbconnector/PGInterface.cpp
	dbconnector/PGToDatumConverter.cpp
	dbconnector/PGValue.cpp
)

list(APPEND MAD_SOURCES ${MAD_DBAL_PG_SOURCES})

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

    set_target_properties(madlib_postgres PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY lib
        BUILD_WITH_INSTALL_RPATH YES
    )

    if(APPLE)
        set_target_properties(madlib_postgres
            PROPERTIES LINK_FLAGS "-bundle_loader ${POSTGRESQL_EXECUTABLE}")
        osx_archs(${POSTGRESQL_EXECUTABLE} MAD_ARCHITECTURES)
        message(STATUS ${POSTGRESQL_EXECUTABLE} " has architectures " ${MAD_ARCHITECTURES})
        set_target_properties(madlib_postgres
            PROPERTIES OSX_ARCHITECTURES "${MAD_ARCHITECTURES}")
        
        # On the Mac, the Accelerate is already an umbrella for everything
        # we need for Armadillo
        target_link_libraries(madlib_postgres "-framework Accelerate")
    else(APPLE)
        # On other platforms, we link to the armadillo library
        target_link_libraries(madlib_postgres armadillo)
        
        # the RPATH to be used when installing. This is set to "$ORIGIN" because
        # the armadillo library will reside in the same directory as the MADlib
        # library
        set_target_properties(madlib_postgres
            PROPERTIES INSTALL_RPATH "\$ORIGIN")
    endif(APPLE)
    
    add_subdirectory(madpack)
else(POSTGRESQL_FOUND)
    message(STATUS "***")
    message(STATUS "*** No PostgreSQL installation found. Skipping.")
    message(STATUS "***")
endif(POSTGRESQL_FOUND)
