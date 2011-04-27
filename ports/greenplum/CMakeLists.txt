set(MAD_DBAL_GP_SOURCES
	AbstractPGValue.cpp
	compatibility.cpp
	main.cpp
	PGAllocator.cpp
	PGInterface.cpp
	PGToDatumConverter.cpp
	PGValue.cpp
)

foreach(FILE ${MAD_DBAL_GP_SOURCES})
	list(APPEND MAD_SOURCES ../postgres/${FILE})
endforeach()

message(STATUS "Trying to find Greenplum installation")
find_package(Greenplum)

if(GREENPLUM_FOUND)
    message(STATUS "***")
    message(STATUS "*** Adding Greenplum to target list...")
    message(STATUS "***")
    
    include_directories(${GREENPLUM_INCLUDE_DIR})
    
    # Directory we will use for patches
    set(PATCH_DIR ${CMAKE_CURRENT_BINARY_DIR}/patch)
    
    # Version 4.1 of Greenplum has a bug in one of its header files that
    # needs to be fixed when building with C++.
    set(_GP_VERSION "${GREENPLUM_VERSION_MAJOR}.${GREENPLUM_VERSION_MINOR}.${GREENPLUM_VERSION_PATCH}")
    if((${_GP_VERSION} VERSION_EQUAL "4.1.0" OR ${_GP_VERSION} VERSION_GREATER "4.1.0")
        AND ${_GP_VERSION} VERSION_LESS "4.2.0")
        
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
    endif((${_GP_VERSION} VERSION_EQUAL "4.1.0" OR ${_GP_VERSION} VERSION_GREATER "4.1.0")
        AND ${_GP_VERSION} VERSION_LESS "4.2.0")

    # Create library
    add_library(
        madlib_greenplum
        MODULE
        ${MAD_SOURCES}
    )
    add_dependencies(madlib_greenplum EP_armadillo)

    set_target_properties(madlib_greenplum
        PROPERTIES PREFIX ""
        OUTPUT_NAME madlib)

    if(APPLE)
        set_target_properties(madlib_greenplum
            PROPERTIES LINK_FLAGS "-bundle_loader ${GREENPLUM_EXECUTABLE}")
        osx_archs(${GREENPLUM_EXECUTABLE} MAD_ARCHITECTURES)
        message(STATUS ${GREENPLUM_EXECUTABLE} " has architectures " ${MAD_ARCHITECTURES})
        set_target_properties(madlib_greenplum
            PROPERTIES OSX_ARCHITECTURES ${MAD_ARCHITECTURES})
        target_link_libraries(madlib_greenplum "-framework Accelerate")
    else(APPLE)
        # On other platforms, we link to the armadillo library
        target_link_libraries(madlib_greenplum armadillo)
        
        # the RPATH to be used when installing. This is set to "$ORIGIN" because
        # the armadillo library will reside in the same directory as the MADlib
        # library
        set_target_properties(madlib_greenplum
            PROPERTIES INSTALL_RPATH "\$ORIGIN")
    endif(APPLE)

    # FIXME: The following 4 lines are for testing purposes only. madpack will
    # do this job later.
    configure_file(${CMAKE_SOURCE_DIR}/extra/regress.py regress.py COPYONLY)
    get_property(MADLIB_SHARED_LIB TARGET madlib_greenplum PROPERTY LOCATION)
    set(MADLIB_PYTHON_PATH ${CMAKE_CURRENT_BINARY_DIR})
    configure_file(${CMAKE_SOURCE_DIR}/extra/regress.sql.in regress.sql)
else(GREENPLUM_FOUND)
    message(STATUS "***")
    message(STATUS "*** No Greenplum installation found. Skipping.")
    message(STATUS "***")
endif(GREENPLUM_FOUND)
