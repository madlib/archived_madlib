# ------------------------------------------------------------------------------
# Packaging for Greenplum's gppkg
# ------------------------------------------------------------------------------

set(MADLIB_GPPKG_RELEASE_NUMBER 1)
set(MADLIB_GPPKG_RPM_FILES
    "${CMAKE_BINARY_DIR}/_CPack_Packages/Linux/RPM/${CPACK_PACKAGE_FILE_NAME}"
)
find_program(
    GPPKG_BINARY
    gppkg
    PATH /usr/local/greenplum-db/bin
    DOC "Path to Greenplum gppkg"
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
    COMMENT "Generating all Greenplum gppkg installers"
)

# We now run generated files, one for each Greenplum version
file(GLOB GPPKG_VERSIONS "${CMAKE_CURRENT_BINARY_DIR}/Version_*.cmake")
foreach(GPPKG_VERSION ${GPPKG_VERSIONS})
    include("${GPPKG_VERSION}")
endforeach(GPPKG_VERSION)
