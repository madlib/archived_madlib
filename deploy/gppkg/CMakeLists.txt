OPTION(GPPKG_BUILD "Enable gppkg build" OFF)

if(GPPKG_BUILD)

    #create RPM specific directories
    FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/SPECS ${CMAKE_CURRENT_BINARY_DIR}/RPMS ${CMAKE_CURRENT_BINARY_DIR}/BUILD)

    #replace platform specific tags
    FILE(COPY gppkg DESTINATION ${CMAKE_CURRENT_BINARY_DIR}) 
    FILE(READ ${CMAKE_CURRENT_BINARY_DIR}/gppkg/gppkg_spec.yml GPPKG_SPECFILE_CONTENTS)
    STRING(REGEX REPLACE "[#]os" "${CMAKE_SYSTEM_NAME}" NEW_GPPKG_SPECFILE_CONTENTS "${GPPKG_SPECFILE_CONTENTS}")
    FILE(WRITE ${CMAKE_CURRENT_BINARY_DIR}/gppkg/gppkg_spec.yml "${NEW_GPPKG_SPECFILE_CONTENTS}")

    add_custom_target(gppkg 
                      COMMAND make package 
                      COMMAND cp ${CMAKE_SOURCE_DIR}/build/madlib-*.rpm gppkg/
                      COMMAND gppkg --build gppkg 
                      COMMAND mv *.gppkg ${CMAKE_SOURCE_DIR}/build
                     )

endif()
