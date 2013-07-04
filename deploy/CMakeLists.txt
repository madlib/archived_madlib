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

set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/description.txt")
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

set(CPACK_RPM_USER_BINARY_SPECFILE ${CMAKE_CURRENT_SOURCE_DIR}/madlib.spec.in)

# -- Set settings for specific package generators ------------------------------

add_subdirectory(PackageMaker)
add_subdirectory(PGXN)
add_subdirectory(RPM)
# gppkg depends on macros from RPM!
add_subdirectory(gppkg)


# -- Finally do the packaging! -------------------------------------------------

set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rpm_post.sh")
set(CPACK_PREFLIGHT_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/preflight.sh)
set(CPACK_POSTFLIGHT_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/postflight.sh)
set(CPACK_MONOLITHIC_INSTALL 1)
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
    DESCRIPTION "DBMS-specific files and libraries."
    EXPANDED
)

file(GLOB PORT_COMPONENTS "${CMAKE_CURRENT_BINARY_DIR}/Component_*.cmake")
foreach(PORT_COMPONENT ${PORT_COMPONENTS})
    include("${PORT_COMPONENT}")
endforeach(PORT_COMPONENT)
