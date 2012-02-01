# Define Greenplum feature macros
#
function(define_greenplum_features IN_VERSION OUT_FEATURES)
    if(NOT ${IN_VERSION} VERSION_LESS "4.1")
        list(APPEND ${OUT_FEATURES} __HAS_ORDERED_AGGREGATES__)
    endif()
    
    # Pass values to caller
    set(${OUT_FEATURES} "${${OUT_FEATURES}}" PARENT_SCOPE)
endfunction(define_greenplum_features)
