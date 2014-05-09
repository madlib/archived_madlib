# Define HAWQ feature macros
#
function(define_hawq_features IN_VERSION OUT_FEATURES)
    list(APPEND ${OUT_FEATURES} __HAS_ORDERED_AGGREGATES__)
    list(APPEND ${OUT_FEATURES} __HAS_FUNCTION_PROPERTIES__)

    # Pass values to caller
    set(${OUT_FEATURES} "${${OUT_FEATURES}}" PARENT_SCOPE)
endfunction(define_hawq_features)

