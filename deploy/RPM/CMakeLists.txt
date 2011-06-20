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
set(CPACK_RPM_PACKAGE_LICENSE "New BSD License" PARENT_SCOPE)
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries" PARENT_SCOPE)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr/local/madlib" PARENT_SCOPE)

set(_PACKAGE_REQUIRES "lapack >= 3.0, m4 >= 1.4")
if(RH_VERSION AND RH_VERSION VERSION_LESS "6.0")
    set(CPACK_RPM_PACKAGE_REQUIRES "python26, ${_PACKAGE_REQUIRES}" PARENT_SCOPE)
else()
    set(CPACK_RPM_PACKAGE_REQUIRES "python >= 2.6, ${_PACKAGE_REQUIRES}" PARENT_SCOPE)
endif()

if(EXISTS "/etc/rpm/macros.python26")
    # On RH 5/CentOS 5, /usr/bin/python is Python 2.4. Unfortunately,
    # /usr/lib/rpm/redhat/macros defines __os_install_post to call
    # /usr/lib/rpm/brp-python-bytecompile without any argument, which then leads
    # to /usr/bin/python being used for byte-compiling. If the python26 package
    # from EPEL is installed, we provide an autmatic fix, so that the RPM build
    # process uses Python 2.6.
    # See https://bugzilla.redhat.com/show_bug.cgi?id=573151#c9 for more
    # information.
    set(CPACK_RPM_SPEC_MORE_DEFINE "%define __os_install_post %{__python26_os_install_post}" PARENT_SCOPE)
endif(EXISTS "/etc/rpm/macros.python26")
