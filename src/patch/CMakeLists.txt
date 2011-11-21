# -- On platforms with a flat namespace for symbols in executables, patch
#    Armadillo so that it calls madlib_{LAPACK/BLAS function} instead of
#    {LAPACK/BLAS function}. This is necessary because some DBMSs contain
#    LAPACK/BLAS symbols, which we do not want to use. Instead, the connector
#    libraries will define madlib_{LAPACK/BLAS function} to look up
#    {LAPACK/BLAS function} dynamically with dlsym() ---------------------------

if(LINUX)
    add_custom_command(
        OUTPUT
            "armadillo"
            "armadillo_bits/compiler_setup.hpp"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${MAD_THIRD_PARTY}/src/EP_armadillo/include/armadillo"
            "armadillo"
        COMMAND ${CMAKE_COMMAND} -E copy
            "${MAD_THIRD_PARTY}/src/EP_armadillo/include/armadillo_bits/compiler_setup.hpp"
            "armadillo_bits/compiler_setup.hpp"
        COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/Armadillo.sh"
        DEPENDS
            "${MAD_THIRD_PARTY}/src/EP_armadillo/include/armadillo"
            "${MAD_THIRD_PARTY}/src/EP_armadillo/include/armadillo_bits/compiler_setup.hpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/Armadillo.sh"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Patching armadillo_bits/compiler_setup.hpp: Route LAPACK/BLAS calls through MADlib connector."
    )
    list(APPEND MADLIB_TARGET_PATCH_FILES
        "${MAD_PATCH_DIR}/armadillo_bits/compiler_setup.hpp"
    )
endif(LINUX)

add_custom_target(madlibPatches ALL DEPENDS ${MADLIB_TARGET_PATCH_FILES})
add_dependencies(madlibPatches EP_armadillo EP_eigen)
