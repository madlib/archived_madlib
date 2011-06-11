# ------------------------------------------------------------------------------
# Packaging with RPM
# ------------------------------------------------------------------------------
#
# Important: Set variables using set(... PARENT_SCOPE), so that the scope of the
# definition extends to the parent scope


# -- Set RPM-specific variables ------------------------------------------------

set(CPACK_RPM_PACKAGE_ARCHITECTURE x86_64 PARENT_SCOPE)
set(CPACK_RPM_PACKAGE_LICENSE "New BSD License" PARENT_SCOPE)
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries" PARENT_SCOPE)
set(CPACK_RPM_PACKAGE_REQUIRES "python >= 2.4, lapack >= 3.0, m4 >= 1.4" PARENT_SCOPE)
