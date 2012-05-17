add_current_greenplum_version()

if(GREENPLUM_4_0_FOUND)
    add_subdirectory(config)

    # Old versions of Greenplum ship with header files that need a patch to be
    # compatible with C++
    add_subdirectory(patch)

    # Append directory with patched header file at beginning
    
    include_directories(BEFORE SYSTEM ${CMAKE_CURRENT_BINARY_DIR}/patch/server)
endif(GREENPLUM_4_0_FOUND)
