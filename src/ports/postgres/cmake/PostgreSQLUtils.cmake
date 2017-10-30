# Define PostgreSQL feature macros
#
function(define_postgresql_features IN_VERSION OUT_FEATURES)
    if(NOT ${IN_VERSION} VERSION_LESS "9.0")
        list(APPEND ${OUT_FEATURES} __HAS_ORDERED_AGGREGATES__)
    endif()

    # Pass values to caller
    set(${OUT_FEATURES} "${${OUT_FEATURES}}" PARENT_SCOPE)
endfunction(define_postgresql_features)

# Add the installer group for this port and a component for all files that are
# not version-specific
#
# We dynamically generate a CMake input file here. This is because the effects
# of cpack_add_component_group() are not globally visible. Hence, we generate
# a file in the deploy directory that CMake will execute only at the very end.
# The motivation is that that way we want to have a clean separation between
# port-specific source code and general code.
#
function(cpack_add_port_group_and_component_for_all_versions)
    file(WRITE "${PORT_DEPLOY_SCRIPT}" "
    cpack_add_component_group(${PORT}
        DISPLAY_NAME \"${PORT} Support\"
        DESCRIPTION \"MADlib support for ${PORT}.\"
        PARENT_GROUP ports
    )
    cpack_add_component(${PORT}_any
        DISPLAY_NAME \"All Versions\"
        DESCRIPTION \"MADlib files shared by all ${PORT} versions.\"
        GROUP ${PORT}
    )")
endfunction(cpack_add_port_group_and_component_for_all_versions)


# Add the installer component for version-specific files
#
function(cpack_add_version_component)
    file(APPEND "${PORT_DEPLOY_SCRIPT}" "
    cpack_add_component(${DBMS}
        DISPLAY_NAME \"${IN_PORT_VERSION}\"
        DESCRIPTION \"MADlib files specific to ${PORT} ${IN_PORT_VERSION}.\"
        GROUP ${PORT}
    )")
endfunction(cpack_add_version_component)


# Determine the versions of this port that we need to build for.
#
# If the user specifies at least one ${PORT_UC}_X_Y_PG_CONFIG, we only build
# for that specific version. If no such variable is defined, we look for any
# version of this port. This function will have a *side effect* in that case:
# It sets one ${PORT_UC}_X_Y_PG_CONFIG to the path to pg_config that was found.
#
function(determine_target_versions OUT_VERSIONS)
    get_subdirectories("${CMAKE_CURRENT_SOURCE_DIR}" SUPPORTED_VERSIONS)
    get_filtered_list(SUPPORTED_VERSIONS "^[0-9]+.*$" ${SUPPORTED_VERSIONS})

    foreach(VERSION ${SUPPORTED_VERSIONS})
        string(REPLACE "." "_" VERSION_UNDERSCORE "${VERSION}")
        if(DEFINED ${PORT_UC}_${VERSION_UNDERSCORE}_PG_CONFIG)
            list(APPEND ${OUT_VERSIONS} ${VERSION})
        endif()
    endforeach(VERSION)
    if(NOT DEFINED ${OUT_VERSIONS})
        find_package(${PORT})
        if(${PORT_UC}_FOUND)
            set(VERSION "${${PORT_UC}_VERSION_MAJOR}.${${PORT_UC}_VERSION_MINOR}")
            if(${PORT_UC} STREQUAL "GREENPLUM")
                # Starting GPDB 5.0, semantic versioning will be followed,
                # implying we only need 1 folder for same major versions
                if(${${PORT_UC}_VERSION_MAJOR} EQUAL 5)
                    set(VERSION "5")
                elseif(${${PORT_UC}_VERSION_MAJOR} EQUAL 6)
                    set(VERSION "6")

                # Due to the ABI incompatibility between 4.3.4 and 4.3.5,
                # MADlib treat 4.3.5+ as DB version that is different from 4.3
                elseif(${${PORT_UC}_VERSION_MAJOR} EQUAL 4 AND
                        ${${PORT_UC}_VERSION_MINOR} EQUAL 3 AND
                        ${${PORT_UC}_VERSION_PATCH} GREATER 4)
                    set(VERSION "4.3ORCA")
                endif()
            elseif(${PORT_UC} STREQUAL "HAWQ" AND
                    ${${PORT_UC}_VERSION_MAJOR} EQUAL 2)
                # Starting HAWQ 2.0, semantic versioning will be followed,
                # implying we only need 1 folder for same major versions
                set(VERSION "2")
            endif()

            list(FIND SUPPORTED_VERSIONS "${VERSION}" _POS)
            if(_POS EQUAL -1)
                string(REPLACE ";" ", " _SUPPORTED_VERSIONS_STR "${SUPPORTED_VERSIONS}")
                message(STATUS "Found pg_config at "
                    "\"${${PORT_UC}_PG_CONFIG}\", but it points to ${PORT} version "
                    "${${PORT_UC}_VERSION_STRING}, which is not one of the supported "
                    "versions (${_SUPPORTED_VERSIONS_STR}). You may try to "
                    "copy a similar version folder in \"src/ports/${PORT_DIR_NAME}\" and "
                    "rename it to \"${VERSION}\". This may or may not work, and is "
                    "unsupported in any case.")
            else(_POS EQUAL -1)
                string(REPLACE "." "_" VERSION_UNDERSCORE "${VERSION}")

                # Side effect:
                set(${PORT_UC}_${VERSION_UNDERSCORE}_PG_CONFIG
                    "${${PORT_UC}_PG_CONFIG}" PARENT_SCOPE)
                set(${OUT_VERSIONS} "${VERSION}")
            endif(_POS EQUAL -1)
        endif(${PORT_UC}_FOUND)
    endif(NOT DEFINED ${OUT_VERSIONS})

    # Pass values to caller
    set(${OUT_VERSIONS} "${${OUT_VERSIONS}}" PARENT_SCOPE)
    # ${PORT_UC}_${_VERSION_UNDERSCORE}_PG_CONFIG might have been set earlier!
    # (the side effect)
endfunction(determine_target_versions)
