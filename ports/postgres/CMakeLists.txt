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

    set_target_properties(madlib_postgres
        PROPERTIES PREFIX "")

    if(APPLE)
        set_target_properties(madlib_postgres
            PROPERTIES LINK_FLAGS "-bundle_loader ${POSTGRESQL_EXECUTABLE}")
        osx_archs(${POSTGRESQL_EXECUTABLE} MAD_ARCHITECTURES)
        set_target_properties(madlib_postgres
            PROPERTIES OSX_ARCHITECTURES ${MAD_ARCHITECTURES})
        target_link_libraries(madlib_postgres "-framework Accelerate")
    endif(APPLE)
else(POSTGRESQL_FOUND)
    message(STATUS "***")
    message(STATUS "*** No PostgreSQL installation found. Skipping.")
    message(STATUS "***")
endif(POSTGRESQL_FOUND)
