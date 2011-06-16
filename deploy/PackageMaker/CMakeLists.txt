# ------------------------------------------------------------------------------
# Packaging with MacOS X PackageMaker
# ------------------------------------------------------------------------------
#
# Important: Set variables using set(... PARENT_SCOPE), so that the scope of the
# definition extends to the parent scope


# -- Set PackageMaker-specific variables ---------------------------------------

set(CPACK_RESOURCE_FILE_README
    "${CPACK_PACKAGE_DESCRIPTION_FILE}" PARENT_SCOPE)
set(CPACK_RESOURCE_FILE_LICENSE
    "${CMAKE_SOURCE_DIR}/license/MADlib.txt" PARENT_SCOPE)
set(CPACK_RESOURCE_FILE_WELCOME
    "${CMAKE_CURRENT_SOURCE_DIR}/Welcome.html" PARENT_SCOPE)
set(CPACK_OSX_PACKAGE_VERSION "10.5" PARENT_SCOPE)
