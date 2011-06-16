# Get the RedHat/CentOS version
macro(rh_version OUT_VERSION)
    if(EXISTS "/etc/redhat-release")
        file(READ "/etc/redhat-release" _REDHAT_RELEASE_CONTENT)
        string(REGEX REPLACE "[^0-9.]*([0-9.]+)[^0-9.]*\$" "\\1" ${OUT_VERSION}
            "${_REDHAT_RELEASE_CONTENT}"
        )
    else(EXISTS "/etc/redhat-release")
        set(${OUT_VERSION} "${OUT_VERSION}-NOTFOUND")
    endif(EXISTS "/etc/redhat-release")
endmacro(rh_version)
