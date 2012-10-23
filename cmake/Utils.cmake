# Define GNU M4 preprocessor macros
#
# Generate M4 code and M4 command-line arguments to define a set of macros that
# will be available to non-compiled source code (SQL, Python).
#
# It is expected that the caller has defined the following variables:
# PORT_UC
# DBMS
# DBMS_UC
# ${DBMS_UC}_VERSION_MAJOR
# ${DBMS_UC}_VERSION_MINOR
# ${DBMS_UC}_VERSION_PATCH
# ${DBMS_UC}_ARCHITECTURE
function(define_m4_macros OUT_M4_CMD_LINE OUT_M4_CODE)
    set(IN_FEATURES ${ARGN})

    set(MACROS
        "__${PORT_UC}__"
        "__PORT__=${PORT_UC}"
        "__DBMS__=${DBMS}"
        "__DBMS_VERSION__=${${DBMS_UC}_VERSION_STRING}"
        "__DBMS_VERSION_MAJOR__=${${DBMS_UC}_VERSION_MAJOR}"
        "__DBMS_VERSION_MINOR__=${${DBMS_UC}_VERSION_MINOR}"
        "__DBMS_VERSION_PATCH__=${${DBMS_UC}_VERSION_PATCH}"
        "__DBMS_ARCHITECTURE__=${${DBMS_UC}_ARCHITECTURE}"
        "__MADLIB_VERSION__=${MADLIB_VERSION_STRING}"
        "__MADLIB_VERSION_MAJOR__=${MADLIB_VERSION_MAJOR}"
        "__MADLIB_VERSION_MINOR__=${MADLIB_VERSION_MINOR}"
        "__MADLIB_VERSION_PATCH__=${MADLIB_VERSION_PATCH}"
        "__MADLIB_GIT_REVISION__=${MADLIB_GIT_REVISION}"
        "__MADLIB_BUILD_TIME__=${MADLIB_BUILD_TIME}"
        "__MADLIB_BUILD_TYPE__=${CMAKE_BUILD_TYPE}"
        "__MADLIB_BUILD_SYSTEM__=${CMAKE_SYSTEM}"
        "__MADLIB_C_COMPILER__=${MADLIB_C_COMPILER}"
        "__MADLIB_CXX_COMPILER__=${MADLIB_CXX_COMPILER}"
        ${IN_FEATURES}
    )
    list_replace("^(.+)$" "-D\\\\1" ${OUT_M4_CMD_LINE} ${MACROS})
    list_replace("^([^=]+)$" "m4_define(`\\\\1')" ${OUT_M4_CODE} ${MACROS})
    list_replace("^([^=]+)=(.+)$" "m4_define(`\\\\1', `\\\\2')" ${OUT_M4_CODE}
        ${${OUT_M4_CODE}})
    string(REGEX REPLACE ";" "\\n" ${OUT_M4_CODE} "${${OUT_M4_CODE}}")

    # Pass values to caller
    set(${OUT_M4_CMD_LINE} "${${OUT_M4_CMD_LINE}}" PARENT_SCOPE)
    set(${OUT_M4_CODE} "${${OUT_M4_CODE}}" PARENT_SCOPE)
endfunction(define_m4_macros)


macro(join_strings OUT_STRING SEPARATOR LIST)
    foreach(ITEM ${LIST})
        if("${${OUT_STRING}}" STREQUAL "")
            set(${OUT_STRING} "${ITEM}")
        else("${${OUT_STRING}}" STREQUAL "")
            set(${OUT_STRING} "${${OUT_STRING}}${SEPARATOR}${ITEM}")
        endif("${${OUT_STRING}}" STREQUAL "")
    endforeach(ITEM ${LIST})
endmacro(join_strings)

macro(get_dir_name OUT_DIR IN_PATH)
    if(${IN_PATH} MATCHES "^.+/[^/]*\$")
        # If the argument for string(REGEX REPLACE does not match the
        # search string, CMake sets the output to the input string
        # This is not what we want, hence the if-block.
        string(REGEX REPLACE "^(.+)/[^/]*\$" "\\1" ${OUT_DIR} "${IN_PATH}")
    else(${IN_PATH} MATCHES "^.+/[^/]*\$")
        set(${OUT_DIR} "")
    endif(${IN_PATH} MATCHES "^.+/[^/]*\$")
endmacro(get_dir_name)

macro(architecture FILENAME OUT_ARCHITECTURE)
    if (APPLE)
        # On Mac OS X, "lipo" is a more reliable way of finding out the
        # architecture of an executable
        osx_archs("${FILENAME}" _ARCHITECTURE)
    else(APPLE)
        execute_process(
            COMMAND file "${FILENAME}"
            OUTPUT_VARIABLE _FILE_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE)

        # Filter out known architectures
        string(REGEX MATCHALL "x86[-_]64|i386|ppc|ppc64" _ARCHITECTURE "${_FILE_OUTPUT}")

        # Normalize (e.g., some vendors use x86-64 instead of x86_64)
        string(REGEX REPLACE "x86[-_]64" "x86_64" _ARCHITECTURE "${_ARCHITECTURE}")
    endif(APPLE)

    list(REMOVE_DUPLICATES _ARCHITECTURE)
    list(LENGTH _ARCHITECTURE _ARCHITECTURE_LENGTH)
    if(_ARCHITECTURE_LENGTH GREATER 1)
        join_strings(_ARCHITECTURES_STRING ", " "${_ARCHITECTURE}")
        message(FATAL_ERROR "Unique word length requested, but "
            "${FILENAME} is fat binary (${_ARCHITECTURES_STRING}).")
    endif(_ARCHITECTURE_LENGTH GREATER 1)
    set(${OUT_ARCHITECTURE} ${_ARCHITECTURE})
endmacro(architecture)

macro(word_length FILENAME OUT_WORD_LENGTH)
    architecture(${FILENAME} _ARCHITECTURE)
    string(REPLACE "ppc" 32 _${OUT_WORD_LENGTH} "${_ARCHITECTURE}")
    string(REPLACE "ppc64" 64 ${OUT_WORD_LENGTH} "${_ARCHITECTURE}")
    string(REPLACE "i386" 32 ${OUT_WORD_LENGTH} "${_ARCHITECTURE}")
    string(REPLACE "x86_64" 64 ${OUT_WORD_LENGTH} "${_ARCHITECTURE}")
endmacro(word_length)

# Given the length of the parameter list, we require named arguments.
# IN_COMMAND can contain "\${CURRENT_PATH}" and "\${OUTFILE}"
# IN_COMMENT can contain "\${CURRENT_FILE}" which will be substituted by the
# current file
macro(batch_add_command
    ARG_NAME_1 IN_TARGET_PREFIX
    ARG_NAME_2 IN_SOURCE_PREFIX
    ARG_NAME_3 IN_TARGET_SUFFIX
    ARG_NAME_4 IN_SOURCE_SUFFIX
    ARG_NAME_5 IN_COMMAND
    ARG_NAME_6 IN_COMMENT
    ARG_NAME_7 OUT_TARGET_FILES
    ARG_NAME_8)

    if( (NOT ("${ARG_NAME_1}" STREQUAL TARGET_PREFIX)) OR
        (NOT ("${ARG_NAME_2}" STREQUAL SOURCE_PREFIX)) OR
        (NOT ("${ARG_NAME_3}" STREQUAL TARGET_SUFFIX)) OR
        (NOT ("${ARG_NAME_4}" STREQUAL SOURCE_SUFFIX)) OR
        (NOT ("${ARG_NAME_5}" STREQUAL RUN)) OR
        (NOT ("${ARG_NAME_6}" STREQUAL COMMENT)) OR
        (NOT ("${ARG_NAME_7}" STREQUAL TARGET_FILE_LIST_REF)) OR
        (NOT ("${ARG_NAME_8}" STREQUAL SOURCE_FILE_LIST)) )

        message(FATAL_ERROR "Missing (or misspelled) arguments given to batch_add_command().")
    endif()
    set(IN_SOURCE_FILE_LIST ${ARGN})

    foreach(CURRENT_FILE ${IN_SOURCE_FILE_LIST})
        get_filename_component(CURRENT_PATH "${IN_SOURCE_PREFIX}${CURRENT_FILE}" ABSOLUTE)

        set(OUTFILE "${IN_TARGET_PREFIX}${CURRENT_FILE}")
        if(NOT ("${IN_SOURCE_SUFFIX}" STREQUAL ""))
            string(REGEX REPLACE "${IN_SOURCE_SUFFIX}\$" "${IN_TARGET_SUFFIX}"
                OUTFILE "${OUTFILE}")
        endif(NOT ("${IN_SOURCE_SUFFIX}" STREQUAL ""))
        get_dir_name(OUTDIR ${OUTFILE})

        string(REPLACE "\${CURRENT_PATH}" "${CURRENT_FILE}" IN_COMMAND "${IN_COMMAND}")
        string(REPLACE "\${OUTFILE}" "${OUTFILE}" IN_COMMAND "${IN_COMMAND}")
        string(REPLACE "\${OUTDIR}" "${OUTDIR}" IN_COMMAND "${IN_COMMAND}")
        string(REPLACE "\${CURRENT_FILE}" "${CURRENT_FILE}" IN_COMMENT "${IN_COMMENT}")

        add_custom_command(OUTPUT "${OUTFILE}"
            ${IN_COMMAND}
            DEPENDS "${CURRENT_PATH}"
            COMMENT "${IN_COMMENT}")

        list(APPEND ${OUT_TARGET_FILES}
            ${OUTFILE})
    endforeach(CURRENT_FILE)
endmacro(batch_add_command)

macro(add_files OUT_TARGET_FILES IN_SOURCE_DIR IN_TARGET_DIR)
    set(IN_SOURCE_FILE_LIST ${ARGN})

    get_filename_component(SOURCE_DIR_ABS "${IN_SOURCE_DIR}" ABSOLUTE)
    get_filename_component(TARGET_DIR_ABS "${IN_TARGET_DIR}" ABSOLUTE)
    set(MADLIB_COPY_COMMAND
        COMMAND "${CMAKE_COMMAND}" -E copy "\"\${CURRENT_PATH}\"" "\"\${OUTFILE}\""
    )
    batch_add_command(
        TARGET_PREFIX "${TARGET_DIR_ABS}/"
        SOURCE_PREFIX "${SOURCE_DIR_ABS}/"
        TARGET_SUFFIX ""
        SOURCE_SUFFIX ""
        RUN "${MADLIB_COPY_COMMAND}"
        COMMENT "Copying \${CURRENT_FILE}."
        TARGET_FILE_LIST_REF ${OUT_TARGET_FILES}
        SOURCE_FILE_LIST ${IN_SOURCE_FILE_LIST}
    )
endmacro(add_files)

macro(get_subdirectories IN_DIRECTORY OUT_SUBDIRS)
    unset(${OUT_SUBDIRS})
    file(GLOB ENTRIES "${IN_DIRECTORY}/*")
    foreach(ITEM ${ENTRIES})
        if(IS_DIRECTORY "${ITEM}")
            get_filename_component(ITEM "${ITEM}" NAME)
            list(APPEND ${OUT_SUBDIRS} "${ITEM}")
        endif(IS_DIRECTORY "${ITEM}")
    endforeach(ITEM)
endmacro(get_subdirectories)

macro(get_min_version OUT_VERSION)
    set(IN_VERSION_LIST ${ARGN})
    list(GET IN_VERSION_LIST 0 ${OUT_VERSION})
    foreach(_VERSION ${IN_VERSION_LIST})
        if("${_VERSION}" VERSION_LESS "${${OUT_VERSION}}")
            set(${OUT_VERSION} "${_VERSION}")
        endif()
    endforeach(_VERSION)
endmacro(get_min_version)

macro(get_max_version OUT_VERSION)
    set(IN_VERSION_LIST ${ARGN})
    list(GET IN_VERSION_LIST 0 ${OUT_VERSION})
    foreach(_VERSION ${IN_VERSION_LIST})
        if("${_VERSION}" VERSION_GREATER "${${OUT_VERSION}}")
            set(${OUT_VERSION} "${_VERSION}")
        endif()
    endforeach(_VERSION)
endmacro(get_max_version)

macro(get_filtered_list OUT_LIST IN_REGEX)
    set(IN_LIST ${ARGN})

    set(${OUT_LIST} "")
    foreach(ITEM ${IN_LIST})
        if("${ITEM}" MATCHES "${IN_REGEX}")
            list(APPEND ${OUT_LIST} "${ITEM}")
        endif("${ITEM}" MATCHES "${IN_REGEX}")
    endforeach(ITEM)
endmacro(get_filtered_list)

macro(list_replace IN_REGEX IN_REPLACE_STRING OUT_LIST)
    set(IN_LIST ${ARGN})

    set(${OUT_LIST})
    foreach(ITEM ${IN_LIST})
        string(REGEX REPLACE "${IN_REGEX}" "${IN_REPLACE_STRING}" ITEM_REPLACED "${ITEM}")
        list(APPEND ${OUT_LIST} "${ITEM_REPLACED}")
    endforeach(ITEM)
endmacro(list_replace)
