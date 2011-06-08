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

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.txt")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Open-Source Library for Scalable in-Database Analytics")
set(CPACK_PACKAGE_FILE_NAME
    "madlib-${MADLIB_VERSION_STRING}-${CMAKE_SYSTEM_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "madlib")
set(CPACK_PACKAGE_NAME "madlib")
set(CPACK_PACKAGE_VENDOR "madlib")
set(CPACK_PACKAGE_VERSION ${MADLIB_VERSION_STRING})
set(CPACK_PACKAGE_VERSION_MAJOR ${MADLIB_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${MADLIB_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${MADLIB_VERSION_PATCH})
set(CPACK_SOURCE_GENERATOR "TGZ;TZ")


# -- Set settings for specific package generators ------------------------------

add_subdirectory(PackageMaker)

message(STATUS ${CPACK_RESOURCE_FILE_README} ";" ${CPACK_RESOURCE_FILE_LICENSE} ";"
    ${CPACK_RESOURCE_FILE_WELCOME})

add_subdirectory(RPM)


# -- Finally do the packaging! -------------------------------------------------

include(CPack)


# -- We can now use CPack commands to do customization -------------------------

cpack_add_component_group(general
    DISPLAY_NAME "MADlib: Generic Components"
    DESCRIPTION "Generic files and core libraries."
    EXPANDED
)

cpack_add_component(doc
    DISPLAY_NAME Documentation
    DESCRIPTION "API reference and documentation (generated with Doxygen)."
    GROUP general
)

cpack_add_component(core
    DISPLAY_NAME "Madlib Core"
    DESCRIPTION "DBMS-independent files installed with every MADlib installation."
    REQUIRED
    GROUP general
)

cpack_add_component_group(ports
    DISPLAY_NAME "DBMS-Specific Components"
    DESCRIPTION "DBMS-specific files and connector libraries."
    EXPANDED
)

# FIXME: Think whether port-specific definitions can be made in the src/ports
# hierarchy
cpack_add_component(postgres
    DISPLAY_NAME "PostgreSQL Database >= 9.0"
    GROUP ports
)

cpack_add_component(greenplum
    DISPLAY_NAME "Greenplum Database >= 4.0"
    GROUP ports
)
