# ------------------------------------------------------------------------------
# Packaging
# ------------------------------------------------------------------------------

# -- Define which package generators to use, depending on the current
#    platform ------------------------------------------------------------------

if(APPLE)
    list(APPEND CPACK_GENERATOR 
        PackageMaker
    )
elseif(UNIX)
    list(APPEND CPACK_GENERATOR
        RPM
    )
endif()


# -- General settings for all/multiple packages generators ---------------------

if(PACKAGE_SUFFIX)
    set(_PACKAGE_SUFFIX "-${PACKAGE_SUFFIX}")
else(PACKAGE_SUFFIX)
    set(_PACKAGE_SUFFIX "")
endif(PACKAGE_SUFFIX)

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/ReadMe_Install.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Open-Source Library for Scalable in-Database Analytics")
set(CPACK_PACKAGE_FILE_NAME
    "madlib${_PACKAGE_SUFFIX}-${MADLIB_VERSION_STRING}-${CMAKE_SYSTEM_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "madlib")
set(CPACK_PACKAGE_NAME "MADlib${_PACKAGE_SUFFIX}")
set(CPACK_PACKAGE_VENDOR "MADlib")
set(CPACK_PACKAGE_VERSION ${MADLIB_VERSION_STRING})
set(CPACK_PACKAGE_VERSION_MAJOR ${MADLIB_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${MADLIB_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${MADLIB_VERSION_PATCH})

# CPACK_PACKAGING_INSTALL_PREFIX has to be set in the generator-specific
# section!

# -- Set settings for specific package generators ------------------------------

add_subdirectory(PackageMaker)
add_subdirectory(RPM)


# -- Finally do the packaging! -------------------------------------------------

include(CPack)


# -- We can now use CPack commands to do customization -------------------------

cpack_add_component(doc
    DISPLAY_NAME Documentation
    DESCRIPTION "API reference and documentation (generated with Doxygen)."
)

cpack_add_component(core
    DISPLAY_NAME "Madlib Core"
    DESCRIPTION "DBMS-independent files installed with every MADlib installation."
    REQUIRED
)

cpack_add_component_group(ports
    DISPLAY_NAME "DBMS-Specific Components"
    DESCRIPTION "DBMS-specific files and connector libraries."
    EXPANDED
)

# FIXME: Think whether port-specific definitions can be made in the src/ports
# hierarchy
cpack_add_component(postgres
    DISPLAY_NAME "PostgreSQL 9.x Connector"
    DESCRIPTION "MADlib connector for PostgreSQL, plus PostgreSQL-specific files. Supports PostgreSQL 9.x."
    GROUP ports
)

cpack_add_component(greenplum
    DISPLAY_NAME "Greenplum 4.x Connector"
    DESCRIPTION "MADlib connector for Greenplum, plus Greenplum-specific files. Supports Greenplum 4.x."
    GROUP ports
)
