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

set(LAPACK_VERSION "3.0")
set(LAPACK_RELEASE "37")
set(BLAS_VERSION "3.0")
set(BLAS_RELEASE "37")

set(LAPACK_URL "ftp://ftp.sunet.se/pub/Linux/distributions/scientific/50/x86_64/SL/lapack-${LAPACK_VERSION}-${LAPACK_RELEASE}.el5.x86_64.rpm")
set(BLAS_URL "ftp://ftp.sunet.se/pub/Linux/distributions/scientific/54/x86_64/SL/blas-${BLAS_VERSION}-${BLAS_RELEASE}.el5.x86_64.rpm")

OPTION(GPPKG_BUILD "Enable gppkg build" OFF)


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

# -- Set settings for specific package generators ------------------------------

if(GPPKG_BUILD)
    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/deploy/lapack.rpm)
        FILE(DOWNLOAD ${LAPACK_URL} ${CMAKE_SOURCE_DIR}/deploy/lapack.rpm)
    endif()

    if(NOT EXISTS ${CMAKE_SOURCE_DIR}/deploy/blas.rpm)
        FILE(DOWNLOAD ${BLAS_URL} ${CMAKE_SOURCE_DIR}/deploy/blas.rpm)
    endif()

    add_custom_target(gppkg 
                      COMMAND make package 
                      COMMAND cp -rf ${CMAKE_SOURCE_DIR}/deploy/package .
                      COMMAND mv lapack-*-*.rpm package/gppkg/
                      COMMAND mv blas-*-*.rpm package/gppkg/
                      COMMAND cp ${CMAKE_SOURCE_DIR}/build/madlib-*.rpm package/gppkg/
                      COMMAND sed -i".bk" 's/os/${CMAKE_SYSTEM_NAME}/g' package/gppkg/gppkg_spec.yml
                      COMMAND rm -f package/gppkg/gppkg_spec.yml.bk
                      COMMAND gppkg --build package/gppkg 
                      COMMAND mv *.gppkg ${CMAKE_SOURCE_DIR}/build
                     )   

    add_custom_target(lapack_rpm
                      COMMAND cp ${CMAKE_SOURCE_DIR}/deploy/lapack.rpm .
                      COMMAND rpm2cpio lapack.rpm | cpio -divu
                      COMMAND rm -rf SPECS RPMS BUILD
                      COMMAND mkdir BUILD SPECS RPMS 
                      COMMAND cp -f ${CMAKE_SOURCE_DIR}/deploy/SPECS/lapack.spec SPECS/
                      COMMAND rpmbuild -bb SPECS/lapack.spec --define "lapackver ${LAPACK_VERSION}" --define "lapackrel ${LAPACK_RELEASE}"
                      COMMAND cp -f ${CMAKE_SOURCE_DIR}/build/deploy/RPMS/*/lapack*.rpm .
                     )

    add_custom_target(blas_rpm
                      COMMAND cp ${CMAKE_SOURCE_DIR}/deploy/blas.rpm .
                      COMMAND rpm2cpio blas.rpm | cpio -divu
                      COMMAND rm -rf SPECS RPMS BUILD
                      COMMAND mkdir SPECS RPMS BUILD
                      COMMAND cp -f ${CMAKE_SOURCE_DIR}/deploy/SPECS/blas.spec SPECS/
                      COMMAND rpmbuild -bb SPECS/blas.spec --define "blasver ${BLAS_VERSION}" --define "blasrel ${BLAS_RELEASE}"
                      COMMAND cp -f ${CMAKE_SOURCE_DIR}/build/deploy/RPMS/*/blas*.rpm . 
                     )

    add_dependencies(gppkg lapack_rpm blas_rpm)
endif()

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
