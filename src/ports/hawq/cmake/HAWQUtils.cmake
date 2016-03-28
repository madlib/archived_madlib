# Define HAWQ feature macros
#
function(define_hawq_features IN_VERSION OUT_FEATURES)
    list(APPEND ${OUT_FEATURES} __HAS_ORDERED_AGGREGATES__)
    list(APPEND ${OUT_FEATURES} __HAS_FUNCTION_PROPERTIES__)
    list(APPEND ${OUT_FEATURES} __UDF_ON_SEGMENT_NOT_ALLOWED__)

    # UDTs cannot be defined in HAWQ versions less than 2.0
    if(${IN_PORT_VERSION} VERSION_LESS "2.0")
        list(APPEND ${OUT_FEATURES} __UDT_NOT_ALLOWED__)
    endif(${IN_PORT_VERSION} VERSION_LESS "2.0")

    # Pass values to caller
    set(${OUT_FEATURES} "${${OUT_FEATURES}}" PARENT_SCOPE)
endfunction(define_hawq_features)

