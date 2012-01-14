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

macro(word_length FILENAME OUT_WORD_LENGTH)
    if(APPLE)
        osx_archs(${FILENAME} _ARCHITECTURE)
        string(REPLACE "ppc" 32 _ARCHITECTURE "${_ARCHITECTURE}")
        string(REPLACE "ppc64" 64 _ARCHITECTURE "${_ARCHITECTURE}")
        string(REPLACE "i386" 32 _ARCHITECTURE "${_ARCHITECTURE}")
        string(REPLACE "x86_64" 64 _ARCHITECTURE "${_ARCHITECTURE}")
    else(APPLE)
        execute_process(
            COMMAND file ${FILENAME}
            OUTPUT_VARIABLE _FILE_OUTPUT
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        string(REGEX MATCHALL "([0-9]+)-bit" _ARCHITECTURE "${_FILE_OUTPUT}")
        string(REGEX REPLACE "([0-9]+)-bit" "\\1" _ARCHITECTURE "${_ARCHITECTURE}")
    endif(APPLE)
    
    list(REMOVE_DUPLICATES _ARCHITECTURE)
    list(LENGTH _ARCHITECTURE _ARCHITECTURE_LENGTH)
    if(_ARCHITECTURE_LENGTH GREATER 1)
        message(FATAL_ERROR "Unique word length requested, but "
            "${FILENAME} is fat binary.")
    endif(_ARCHITECTURE_LENGTH GREATER 1)
    set(${OUT_WORD_LENGTH} ${_ARCHITECTURE})
endmacro(word_length)

#marco(map IN_LIST OUT_LIST IN_MAP_FUNCTION)
#    set(${OUT_LIST} "")
#    foreach()
#endmacro(map)

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
