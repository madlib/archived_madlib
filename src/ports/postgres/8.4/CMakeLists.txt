add_current_postgresql_version()

if(POSTGRESQL_8_4_FOUND)
    add_subdirectory(config)

    # Old versions of PostgreSQL ship with header files that need a patch to be
    # compatible with C++
    add_subdirectory(patch)

    # Append directory with patched header file at beginning
    
    include_directories(BEFORE SYSTEM ${CMAKE_CURRENT_BINARY_DIR}/patch/server)
endif(POSTGRESQL_8_4_FOUND)

