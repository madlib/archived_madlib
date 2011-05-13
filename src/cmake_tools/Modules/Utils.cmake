macro(join_strings OUT_STRING SEPARATOR LIST)
    foreach(ITEM ${LIST})
        if("${${OUT_STRING}}" EQUAL "")
            set(${OUT_STRING} "${ITEM}")
        else("${${OUT_STRING}}" EQUAL "")
            set(${OUT_STRING} "${${OUT_STRING}}${SEPARATOR}${ITEM}")
        endif("${${OUT_STRING}}" EQUAL "")
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
