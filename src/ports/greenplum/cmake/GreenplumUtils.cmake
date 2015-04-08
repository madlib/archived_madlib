# Define Greenplum feature macros
#
function(define_greenplum_features IN_VERSION OUT_FEATURES)
    if(NOT ${IN_VERSION} VERSION_LESS "4.1")
        list(APPEND ${OUT_FEATURES} __HAS_ORDERED_AGGREGATES__)
    endif()

    if(NOT ${IN_VERSION} VERSION_LESS "4.3")
        list(APPEND ${OUT_FEATURES} __HAS_FUNCTION_PROPERTIES__)
    endif()

    # Pass values to caller
    set(${OUT_FEATURES} "${${OUT_FEATURES}}" PARENT_SCOPE)
endfunction(define_greenplum_features)

function(add_gppkg GPDB_VERSION GPDB_VARIANT GPDB_VARIANT_SHORT UPGRADE_SUPPORT)
    string(TOLOWER ${GPDB_VERSION} GPDB_VERSION_LC)
    string(REPLACE "." "_" VERSION_ "${GPDB_VERSION}")

    file(WRITE "${CMAKE_BINARY_DIR}/deploy/gppkg/${GPDB_VARIANT}_${VERSION_}_gppkg.cmake" "
    file(MAKE_DIRECTORY
        \"\${CMAKE_CURRENT_BINARY_DIR}/${GPDB_VERSION}/BUILD\"
        \"\${CMAKE_CURRENT_BINARY_DIR}/${GPDB_VERSION}/SPECS\"
        \"\${CMAKE_CURRENT_BINARY_DIR}/${GPDB_VERSION}/RPMS\"
        \"\${CMAKE_CURRENT_BINARY_DIR}/${GPDB_VERSION}/gppkg\"
    )

    set(GPDB_VERSION \"${GPDB_VERSION}\")
    set(GPDB_VERSION_LC \"${GPDB_VERSION_LC}\")
    set(GPDB_VARIANT \"${GPDB_VARIANT}\")
    set(GPDB_VARIANT_SHORT \"${GPDB_VARIANT_SHORT}\")
    set(UPGRADE_SUPPORT \"${UPGRADE_SUPPORT}\")
    string(TOLOWER \"${GPDB_VARIANT}\" PORT_NAME)

    configure_file(
        madlib.spec.in
        \"\${CMAKE_CURRENT_BINARY_DIR}/${GPDB_VERSION}/SPECS/madlib.spec\"
    )
    configure_file(
        gppkg_spec.yml.in
        \"\${CMAKE_CURRENT_BINARY_DIR}/${GPDB_VERSION}/gppkg/gppkg_spec.yml\"
    )

    if(GPPKG_BINARY AND RPMBUILD_BINARY)
        add_custom_target(gppkg_${GPDB_VARIANT}_${VERSION_}
            COMMAND cmake -E create_symlink \"\${MADLIB_GPPKG_RPM_SOURCE_DIR}\"
                \"\${CPACK_PACKAGE_FILE_NAME}-gppkg\"
            COMMAND \"\${RPMBUILD_BINARY}\" -bb SPECS/madlib.spec
            COMMAND cmake -E rename \"RPMS/\${MADLIB_GPPKG_RPM_FILE_NAME}\"
                \"gppkg/\${MADLIB_GPPKG_RPM_FILE_NAME}\"
            COMMAND \"\${GPPKG_BINARY}\" --build gppkg
            DEPENDS \"${CMAKE_BINARY_DIR}/\${CPACK_PACKAGE_FILE_NAME}.rpm\"
            WORKING_DIRECTORY \"\${CMAKE_CURRENT_BINARY_DIR}/${GPDB_VERSION}\"
            COMMENT \"Generating ${GPDB_VARIANT} ${GPDB_VERSION} gppkg installer...\"
            VERBATIM
        )
    else(GPPKG_BINARY AND RPMBUILD_BINARY)
        add_custom_target(gppkg_${GPDB_VARIANT}_${VERSION_}
            COMMAND cmake -E echo \"Could not find gppkg and/or rpmbuild.\"
                \"Please rerun cmake.\"
        )
    endif(GPPKG_BINARY AND RPMBUILD_BINARY)

    # Unfortunately, we cannot set a dependency to the built-in package target,
    # i.e., the following does not work:
    # add_dependencies(gppkg package)

    add_dependencies(gppkg gppkg_${GPDB_VARIANT}_${VERSION_})
    ")
endfunction(add_gppkg)

