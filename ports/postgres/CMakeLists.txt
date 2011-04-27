set(MAD_DBAL_PG_SOURCES
	AbstractPGValue.cpp
	compatibility.cpp
	main.cpp
	PGAllocator.cpp
	PGInterface.cpp
	PGToDatumConverter.cpp
	PGValue.cpp
)

list(APPEND MAD_SOURCES ${MAD_DBAL_PG_SOURCES})

message(STATUS "Trying to find PostgreSQL installation")
find_package(PostgreSQL)

if(POSTGRESQL_FOUND)
    message(STATUS "***")
    message(STATUS "*** Adding PostgreSQL to target list...")
    message(STATUS "***")

    include_directories(${POSTGRESQL_INCLUDE_DIR})

    # Create library
    add_library(
        madlib_postgres
        MODULE
        ${MAD_SOURCES}
    )
    add_dependencies(madlib_postgres EP_armadillo)

    set_target_properties(madlib_postgres
        PROPERTIES PREFIX ""
        OUTPUT_NAME madlib)

    if(APPLE)
        set_target_properties(madlib_postgres
            PROPERTIES LINK_FLAGS "-bundle_loader ${POSTGRESQL_EXECUTABLE}")
        osx_archs(${POSTGRESQL_EXECUTABLE} MAD_ARCHITECTURES)
        message(STATUS ${POSTGRESQL_EXECUTABLE} " has architectures " ${MAD_ARCHITECTURES})
        set_target_properties(madlib_postgres
            PROPERTIES OSX_ARCHITECTURES ${MAD_ARCHITECTURES})
        
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

    # FIXME: The following 4 lines are for testing purposes only. madpack will
    # do this job later.
    configure_file(${CMAKE_SOURCE_DIR}/extra/regress.py regress.py COPYONLY)
    get_property(MADLIB_SHARED_LIB TARGET madlib_postgres PROPERTY LOCATION)
    set(MADLIB_PYTHON_PATH ${CMAKE_CURRENT_BINARY_DIR})
    configure_file(${CMAKE_SOURCE_DIR}/extra/regress.sql.in regress.sql)
else(POSTGRESQL_FOUND)
    message(STATUS "***")
    message(STATUS "*** No PostgreSQL installation found. Skipping.")
    message(STATUS "***")
endif(POSTGRESQL_FOUND)
