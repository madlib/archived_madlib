# ------------------------------------------------------------------------------
# MADlib Documentation
# ------------------------------------------------------------------------------

set(MADLIB_DOXYGEN_SOURCE_DIR "\"${CMAKE_SOURCE_DIR}/src\" \"${CMAKE_SOURCE_DIR}/methods\"")
set(USE_MATHJAX NO CACHE BOOL "In user documentation, render LaTeX formulas using MathJax")
set(MATHJAX_RELPATH "${CMAKE_BINARY_DIR}/third_party/downloads/mathjax" CACHE PATH
    "Path to MathJax installation")


# -- Build doxysql (the SQL parser) using flex and bison -----------------------

find_package(FLEX 2.5)
find_package(BISON 2.4)
find_package(Doxygen)

if(FLEX_FOUND AND BISON_FOUND AND DOXYGEN_FOUND)
    BISON_TARGET(doxysqlParser src/sql.yy ${CMAKE_CURRENT_BINARY_DIR}/sql.parser.cc)
    FLEX_TARGET(doxysqlScanner src/sql.ll ${CMAKE_CURRENT_BINARY_DIR}/sql.scanner.cc)
    ADD_FLEX_BISON_DEPENDENCY(doxysqlScanner doxysqlParser)

    include_directories(${CMAKE_CURRENT_BINARY_DIR})
    add_executable(doxysql ${BISON_doxysqlParser_OUTPUTS}
        ${FLEX_doxysqlScanner_OUTPUTS})
    set_target_properties(doxysql PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")


# -- Copy executable and configuration files -----------------------------------

    add_subdirectory(bin)
    add_subdirectory(etc)


# -- Run doxygen ---------------------------------------------------------------

    set(_DOXYGEN_UNNEEDED_WARNINGS_FILTER egrep -v
        "warning:.*\\(@param is not found in the argument list.*kwargs\\)\$|The following parameters.*kwargs\\) are not documented\\)|parameter 'kwargs'\$")
    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} etc/user.doxyfile | ${_DOXYGEN_UNNEEDED_WARNINGS_FILTER}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS doxysql doxyBinFiles
        VERBATIM
    )
    
    add_custom_target(devdoc
        COMMAND bin/update_mathjax.sh
        COMMAND ${DOXYGEN_EXECUTABLE} etc/developer.doxyfile | ${_DOXYGEN_UNNEEDED_WARNINGS_FILTER}
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        DEPENDS doxysql doxyBinFiles
    )
    
# -- Install doc/user/html output directory to doc/html ------------------------

    # We specify OPTIONAL, which means it will not be an error if the user
    # documentation does not exist
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/user/html
        DESTINATION doc
        OPTIONAL
        COMPONENT doc
        PATTERN ".DS_Store" EXCLUDE 
    )


# -- Notify user if we could not run doxygen

else(FLEX_FOUND AND BISON_FOUND AND DOXYGEN_FOUND)
    message(STATUS "Could not find recent versions of at least one of flex, "
        "bison, doxygen, or dot (part of graphviz, needed for doxygen). "
        "Documentation will not be built.")
endif(FLEX_FOUND AND BISON_FOUND AND DOXYGEN_FOUND)
