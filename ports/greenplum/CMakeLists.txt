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
find_package(Greenplum REQUIRED)

if(GREENPLUM_FOUND)
    message(STATUS "***")
    message(STATUS "*** Adding Greenplum to target list...")
    message(STATUS "***")
    
    include_directories(${GREENPLUM_INCLUDE_DIR})
    
    # Directory we will use for patches
    set(PATCH_DIR ${CMAKE_CURRENT_BINARY_DIR}/patch)
    
    # Version 4.1.0 of Greenplum has a bug in one of its header files that
    # needs to be fixed when building with C++.
    if("${GREENPLUM_VERSION_MAJOR}.${GREENPLUM_VERSION_MINOR}.${GREENPLUM_VERSION_PATCH}"
        VERSION_EQUAL "4.1.0")
        
        message(STATUS "Installed Greenplum version has bug in header file. Applying patch...")
        
        # The first element in GREENPLUM_INCLUDE_DIR is include/server
        list(GET GREENPLUM_INCLUDE_DIR 0 GREENPLUM_SERVER_INCLUDE_DIR)
        
        file(COPY ${GREENPLUM_SERVER_INCLUDE_DIR}/utils/syncbitvector.h DESTINATION
            ${PATCH_DIR}/server/utils)
        execute_process(
            COMMAND ${MAD_BUILD_TOOLS}/greenplum_patch_4_1_0.sh
            WORKING_DIRECTORY ${PATCH_DIR}
        )
        
        # Append directory with patched header file at beginning
        include_directories(BEFORE ${PATCH_DIR}/server)
    endif("${GREENPLUM_VERSION_MAJOR}.${GREENPLUM_VERSION_MINOR}.${GREENPLUM_VERSION_PATCH}"
        VERSION_EQUAL "4.1.0")

    # Create library
    add_library(
        madlib_greenplum
        MODULE
        ${MAD_SOURCES}
    )

    set_target_properties(madlib_greenplum
        PROPERTIES PREFIX "")

    if(APPLE)
        set_target_properties(madlib_greenplum
            PROPERTIES LINK_FLAGS "-bundle_loader ${GREENPLUM_EXECUTABLE}")
        osx_archs(${GREENPLUM_EXECUTABLE} MAD_ARCHITECTURES)
        message(STATUS ${GREENPLUM_EXECUTABLE} " has architectures " ${MAD_ARCHITECTURES})
        set_target_properties(madlib_greenplum
            PROPERTIES OSX_ARCHITECTURES ${MAD_ARCHITECTURES})
        target_link_libraries(madlib_greenplum "-framework Accelerate")
    endif(APPLE)
else(GREENPLUM_FOUND)
    message(STATUS "***")
    message(STATUS "*** No Greenplum installation found. Skipping.")
    message(STATUS "***")
endif(GREENPLUM_FOUND)
