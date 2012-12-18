#include <dbconnector/dbconnector.hpp>

#include "BitmapUtil.hpp"
#include "bitmap.hpp"

namespace madlib {
namespace modules {
namespace bitmap {

/**
 * @brief the step function for the bitmap_agg.
 */
AnyType
bitmap_agg_sfunc::run(AnyType &args){
    RETURN_BITMAP(BitmapUtil::bitmap_agg_sfunc<int32_t>(args));
}

/**
 * @brief the pre-function for the bitmap_agg.
 */
AnyType
bitmap_agg_pfunc::run(AnyType &args){
    RETURN_BITMAP_NULL(BitmapUtil::bitmap_agg_pfunc<int32_t>(args));
}


/**
 * @brief the operator "AND" implementation
 */
AnyType
bitmap_and::run(AnyType &args){
    RETURN_BITMAP_NULL(BitmapUtil::bitmap_and<int32_t>(args));
}


/**
 * @brief the operator "OR" implementation
 */
AnyType
bitmap_or::run(AnyType &args){
    RETURN_BITMAP(BitmapUtil::bitmap_or<int32_t>(args));
}


/**
 * @brief the operator "XOR" implementation
 */
AnyType
bitmap_xor::run(AnyType &args){
    RETURN_BITMAP_NULL(BitmapUtil::bitmap_xor<int32_t>(args));
}


/**
 * @brief the operator "NOT" implementation
 */
AnyType
bitmap_not::run(AnyType &args){
    RETURN_BITMAP_NULL(BitmapUtil::bitmap_not<int32_t>(args));
}


/**
 * @brief set the specified bit to 1/0
 */
AnyType
bitmap_set::run(AnyType &args){
    RETURN_BITMAP(BitmapUtil::bitmap_set<int32_t>(args));
}


/**
 * @brief test whether the specified bit is 1
 */
AnyType
bitmap_test::run(AnyType &args){
    RETURN_BASE(BitmapUtil::bitmap_test<int32_t>(args));
}

/**
 * @brief get the number of bits whose value is 1.
 */
AnyType
bitmap_nonzero_count::run(AnyType &args){
    RETURN_BASE(BitmapUtil::bitmap_nonzero_count<int32_t>(args));
}


/**
 * @brief get the positions of the non-zero bits.
 * @note the position start from 1.
 */
AnyType
bitmap_nonzero_positions::run(AnyType &args){
    RETURN_INT8_ARRAY(BitmapUtil::bitmap_nonzero_positions<int32_t>(args));
}


/**
 * @brief get the bitmap representation for the int64 array
 */
AnyType
bitmap_from_int8array::run(AnyType &args){
    RETURN_BITMAP_NULL((BitmapUtil::bitmap_from_array<int32_t, int64_t>(args)));
}


/**
 * @brief get the bitmap representation for the int32 array
 */
AnyType
bitmap_from_int4array::run(AnyType &args){
    RETURN_BITMAP_NULL((BitmapUtil::bitmap_from_array<int32_t, int32_t>(args)));
}


/**
 * @brief get the bitmap representation for the varbit
 */
AnyType
bitmap_from_varbit::run(AnyType &args){
    RETURN_BITMAP_NULL((BitmapUtil::bitmap_from_varbit<int32_t>(args)));
}


/**
 * @brief the in function for the bitmap
 */
AnyType
bitmap_in::run(AnyType &args){
    RETURN_BITMAP(BitmapUtil::bitmap_in<int32_t>(args));
}


/**
 * @brief the out function for the bitmap
 */
AnyType
bitmap_out::run(AnyType &args){
    RETURN_BASE(BitmapUtil::bitmap_out<int32_t>(args));
}


/**
 * @brief cast the specified varbit to the bitmap representation
 */
AnyType
bitmap_return_varbit::run(AnyType &args){
    RETURN_BASE(BitmapUtil::bitmap_return_varbit<int32_t>(args));
}


/**
 * @brief return the internal representation (array) of the bitmap
 */
AnyType
bitmap_return_array::run(AnyType &args){
    RETURN_INT4_ARRAY(BitmapUtil::bitmap_return_array<int32_t>(args));
}


void*
bitmap_unnest::SRF_init(AnyType &args) {
    return BitmapUtil::bitmap_unnest_init<int32_t>(args);
}

AnyType
bitmap_unnest::SRF_next(void *user_fctx, bool *is_last_call) {
    int64_t res = BitmapUtil::bitmap_unnest_next<int32_t>(user_fctx, is_last_call);
    return -1 == res ? AnyType() : AnyType(res);
}


/**
 * @brief the implementation of operator =
 */
AnyType
bitmap_eq::run(AnyType &args){
    RETURN_BASE(BitmapUtil::bitmap_eq<int32_t>(args));
}


/**
 * @brief the implementation of operator !=
 */
AnyType
bitmap_neq::run(AnyType &args){
    RETURN_BASE(!BitmapUtil::bitmap_eq<int32_t>(args));
}


/**
 * @brief the implementation of operator >
 */
AnyType
bitmap_gt::run(AnyType &args){
    RETURN_BASE(BitmapUtil::bitmap_gt<int32_t>(args));
}


/**
 * @brief the implementation of operator <
 */
AnyType
bitmap_lt::run(AnyType &args){
    RETURN_BASE(!BitmapUtil::bitmap_ge<int32_t>(args));
}


/**
 * @brief the implementation of operator >=
 */
AnyType
bitmap_ge::run(AnyType &args){
    RETURN_BASE(BitmapUtil::bitmap_ge<int32_t>(args));
}


/**
 * @brief the implementation of operator <=
 */
AnyType
bitmap_le::run(AnyType &args){
    RETURN_BASE(!BitmapUtil::bitmap_gt<int32_t>(args));
}


/**
 * @brief compare the two bitmaps
 */
AnyType
bitmap_cmp::run(AnyType &args){
    RETURN_BASE(BitmapUtil::bitmap_cmp<int32_t>(args));
}

} // bitmap
} // modules
} // madlib
