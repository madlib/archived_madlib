# ------------------------------------------------------------------------------
# Packaging for Greenplum's gppkg
# ------------------------------------------------------------------------------

set(MADLIB_GPPKG_RELEASE_NUMBER 1)

set(MADLIB_GPPKG_RPM_SOURCE_DIR
    "${CMAKE_BINARY_DIR}/_CPack_Packages/Linux/RPM/${CPACK_PACKAGE_FILE_NAME}"
)
# gppkg expects that the file name for the RPM from which the gppkg is generated
# follows the pattern "<name>-<version>-<release>.<arch>.rpm". Otherwise,
# uninstallation will not work (MPP-18078). Note that <version> has to be
# consistent with the version in madlib.spec.in. gppkg deduces the
# uninstallation command line options from the filename!
set(MADLIB_GPPKG_RPM_FILE_NAME
    "madlib-${MADLIB_VERSION_STRING_NO_HYPHEN}-${MADLIB_GPPKG_RELEASE_NUMBER}.${CPACK_RPM_PACKAGE_ARCHITECTURE}.rpm")

find_program(
    GPPKG_BINARY
    gppkg
    PATH /usr/local/greenplum-db/bin
    DOC "Path to gppkg"
)
find_program(
    RPMBUILD_BINARY
    rpmbuild
    DOC "Path to rpmbuild"
)
file(READ "${CPACK_PACKAGE_DESCRIPTION_FILE}" CPACK_RPM_PACKAGE_DESCRIPTION)

# The target gppkg is a meta target that depends on all version-specific
# gppkg targets
add_custom_target(gppkg
    COMMENT "Generating all gppkg installers"
)

# We now run generated files, one for each Greenplum version
file(GLOB GPPKG_VERSIONS "${CMAKE_CURRENT_BINARY_DIR}/*_gppkg.cmake")
foreach(GPPKG_VERSION ${GPPKG_VERSIONS})
    include("${GPPKG_VERSION}")
endforeach(GPPKG_VERSION)

set(CPACK_RPM_SPEC_MORE_DEFINE "%undefine __os_install_post" PARENT_SCOPE)
