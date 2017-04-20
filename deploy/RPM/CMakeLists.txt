# ------------------------------------------------------------------------------
# Packaging with RPM
# ------------------------------------------------------------------------------
#
# Important: Set variables using set(... PARENT_SCOPE), so that the scope of the
# definition extends to the parent scope


# Get information about the environment
rh_version(RH_VERSION)

# -- Set RPM-specific variables ------------------------------------------------

set(CPACK_RPM_PACKAGE_ARCHITECTURE x86_64 PARENT_SCOPE)
set(CPACK_RPM_PACKAGE_LICENSE "ASL 2.0" PARENT_SCOPE)
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries" PARENT_SCOPE)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local/madlib/Versions/${MADLIB_VERSION_STRING}" PARENT_SCOPE)

set(_PACKAGE_REQUIRES "m4 >= 1.4")
if(RH_VERSION AND RH_VERSION VERSION_LESS "6.0")
    # on RH/CentOS 5, there is no Python 2.6 or higher in the default
    # repositories.
    set(CPACK_RPM_PACKAGE_REQUIRES "python, ${_PACKAGE_REQUIRES}" PARENT_SCOPE)
else()
    set(CPACK_RPM_PACKAGE_REQUIRES "python >= 2.6, ${_PACKAGE_REQUIRES}" PARENT_SCOPE)
endif()

# We do all deployment preparation with our CMake build script, so we do
# not want to do any post-processing as part of the RPM generation.
set(CPACK_RPM_SPEC_MORE_DEFINE "%undefine __os_install_post" PARENT_SCOPE)

