add_custom_target(madlibPatches ALL DEPENDS ${MADLIB_TARGET_PATCH_FILES})
add_dependencies(madlibPatches EP_eigen)
