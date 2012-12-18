#ifndef MADLIB_MODULES_BITMAP_BITMAP_UTILITY_HPP
#define MADLIB_MODULES_BITMAP_BITMAP_UTILITY_HPP

#include <dbconnector/dbconnector.hpp>

#include "Bitmap_proto.hpp"
#include "Bitmap_impl.hpp"

namespace madlib {
namespace modules {
namespace bitmap {

using madlib::dbconnector::postgres::TypeTraits;
using madlib::dbconnector::postgres::madlib_get_typlenbyvalalign;

/**
 * @brief This class encapsulates the interfaces for manipulating the bitmap.
 *        All functions are static.
 *
 */
class BitmapUtil{
protected:
    enum BITMAPOP {EQ = 0, GT = 1, LT = -1} ;
public:

/**
 * @brief the step function for aggregating the input numbers to a bitmap.
 *
 * @param args[0]   the array indicating the state
 * @param args[1]   the input number
 * @param args[2]   the number of empty elements will be dynamically
 *                  added to the state array. The default value is 16
 *
 * @return the array indicating the state after inserted the input number.
 * @note the bitmap(UDT) uses an integer array as underlying storage. Therefore,
 *       we need to skip the type checks of the C++ AL:
 *          + if the input argument is bitmap, we will get it as array.
 *          + if the return value is bitmap, we will return it as array.
 *
 */
template<typename T>
static
const ArrayType*
bitmap_agg_sfunc
(
    AnyType &args
){
    madlib_assert(!args[1].isNull(),
            std::invalid_argument("the value of the first parameter "
                    "should not be null"));
    int64_t input_bit = args[1].getAs<int64_t>();

    // the default value for this parameter
    int size_per_add = DEFAULT_SIZE_PER_ADD;
    if (3 == args.numFields()){
        madlib_assert(!args[2].isNull(),
                std::invalid_argument("the value of the third parameter "
                        "should not be null"));
        size_per_add = args[2].getAs<int32_t>();
        madlib_assert(size_per_add > 1,
                std::invalid_argument("the input parameter size_per_add "
                        "should no less than 2"));

    }

    AnyType state = args[0];

    if (state.isNull()){
        Bitmap<T> bitmap(size_per_add, size_per_add);
        bitmap.insert(input_bit);
        return bitmap.to_ArrayType();
    }
    // state is not null
    Bitmap<T> bitmap(GETARG_MUTABLE_BITMAP(state), size_per_add);
    bitmap.insert(input_bit);
    return bitmap.updated() ? bitmap.to_ArrayType() : GETARG_IMMUTABLE_BITMAP(state).array();
}


/**
 * @brief the pre-function for the bitmap_agg.
 *
 * @param args[0]   the first state
 * @param args[1]   the second state
 *
 * @return an array of merging the first state and the second state.
 *
 */
template <typename T>
static
const ArrayType*
bitmap_agg_pfunc
(
    AnyType &args
){
    AnyType states[] = {args[0], args[1]};

    int nonull_index = states[0].isNull() ?
            (states[1].isNull() ? -1 : 1) : (states[1].isNull() ? 0 : 2);

    // if the two states are all null, return one of them
    if (nonull_index < 0)
        return NULL;

    // one of the arguments is null
    // trim the zero elements in the non-null bitmap
    if (nonull_index < 2){
        // no need to increase the bitmap size
        Bitmap<T> bitmap(GETARG_MUTABLE_BITMAP(states[nonull_index]));
        if (bitmap.full()){
            return GETARG_IMMUTABLE_BITMAP(states[nonull_index]).array();
        }
        return bitmap.to_ArrayType(false);
    }

    // all the arguments are not null
    // the two state-arrays can be written without copying it
    Bitmap<T> bm1(GETARG_MUTABLE_BITMAP(states[0]));
    Bitmap<T> bm2(GETARG_MUTABLE_BITMAP(states[1]));
    return bm1.op_or(bm2);
}


/**
 * @brief The implementation of operator &.
 *
 * @param args[0]   the first bitmap
 * @param args[1]   the second bitmap
 *
 * @return the result of args[0] & args[1].
 *
 */
template<typename T>
static
const ArrayType*
bitmap_and
(
    AnyType &args
){
    Bitmap<T> bm1(GETARG_IMMUTABLE_BITMAP(args[0]));
    Bitmap<T> bm2(GETARG_IMMUTABLE_BITMAP(args[1]));
    return bm1.op_and(bm2);
}


/**
 * @brief The implementation of operator ^.
 *
 * @param args[0]   the first bitmap
 * @param args[1]   the second bitmap
 *
 * @return the result of args[0] ^ args[1].
 *
 */
template<typename T>
static
const ArrayType*
bitmap_xor
(
    AnyType &args
){
    Bitmap<T> bm1(GETARG_IMMUTABLE_BITMAP(args[0]));
    Bitmap<T> bm2(GETARG_IMMUTABLE_BITMAP(args[1]));
    return bm1.op_xor(bm2);
}


/**
 * @brief the implementation of operator ~
 *
 * @param args[0] the bitmap
 *
 * @return the result of ~args[0]
 */
template<typename T>
static
const ArrayType*
bitmap_not
(
    AnyType &args
){
    Bitmap<T> bm(GETARG_IMMUTABLE_BITMAP(args[0]));
    return bm.op_not();
}

/**
 * @brief The implementation of operator |
 *
 * @param args[0]   the first bitmap
 * @param args[1]   the second bitmap
 *
 * @return the result of args[0] | args[1].
 *
 */
template<typename T>
static
const ArrayType*
bitmap_or
(
    AnyType &args
){
    Bitmap<T> bm1(GETARG_IMMUTABLE_BITMAP(args[0]));
    Bitmap<T> bm2(GETARG_IMMUTABLE_BITMAP(args[1]));
    return bm1.op_or(bm2);
}


/*
 * @brief set the value of the bit with the specified position to 1/0
 *
 * @param args[0]   the bitmap
 * @param args[1]   the position of the bit
 * @param args[2]   true for 1; false for 0
 *
 * @return the changed bitmap
 */
template<typename T>
static
const ArrayType*
bitmap_set
(
    AnyType &args
){

    madlib_assert(!args[0].isNull() && !args[1].isNull() && !args[2].isNull(),
            std::invalid_argument("the input parameters should not be null"));

    int64_t number = args[1].getAs<int64_t>();
    madlib_assert(number > 0,
            std::invalid_argument("the input number should be greater than 0"));
    bool needset = args[2].getAs<bool>();

    Bitmap<T> bm(GETARG_CLONED_BITMAP(args[0]));

    return needset ? bm.insert(number).to_ArrayType(false) : bm.reset(number);
}


/*
 * @brief Test whether the specified bit is set
 *
 * @param args[0]   the bitmap
 * @param args[1]   the specified bit to be tested
 *
 * @return True if the bit with the specified position in the bitmap is set,
 *         otherwise, return False;
 */
template<typename T>
static
const bool
bitmap_test
(
    AnyType &args
){
    madlib_assert(!args[0].isNull() && !args[1].isNull(),
            std::invalid_argument("the input parameters should not be null"));

    int64_t number = args[1].getAs<int64_t>();
    madlib_assert(number > 0,
            std::invalid_argument("the input number should be greater than 0"));

    Bitmap<T> bm(GETARG_MUTABLE_BITMAP(args[0]));
    return bm[number];
}


/**
 * @brief get the number of bits whose value is 1.
 *
 * @param args[0]   the bitmap array
 *
 * @return the count of non-zero bits in the bitmap.
 *
 */
template<typename T>
static
const int64_t
bitmap_nonzero_count
(
    AnyType &args
){
    return (Bitmap<T>(GETARG_IMMUTABLE_BITMAP(args[0]))).nonzero_count();
}


/**
 * @brief get the positions of the non-zero bits
 *
 * @param args[0]   the bitmap array
 *
 * @return the array contains the positions of the non-zero bits.
 * @note the position starts from 1.
 *
 */
template <typename T>
static
const ArrayType*
bitmap_nonzero_positions
(
    AnyType &args
){

    return (Bitmap<T>(GETARG_IMMUTABLE_BITMAP(args[0]))).nonzero_positions();
}


/**
 * @brief get the bitmap representation for the input array.
 *
 * @param args[0]   the input array.
 *
 * @return the bitmap for the input array.
 * @note T is the type of the bitmap
 *       X is the type of the input array
 */
template <typename T, typename X>
static
const ArrayType*
bitmap_from_array
(
    AnyType &args
){
    MutableArrayHandle<X> handle = args[0].getAs< MutableArrayHandle<X> >();
    madlib_assert(!ARR_HASNULL(handle.array()),
            std::invalid_argument("the input array should not contains null"));

    X* array = handle.ptr();
    int size = handle.size();

    if (0 == size)
        return NULL;

    qsort(array, size, sizeof(X), compare<X>);
    Bitmap<T> bitmap(DEFAULT_SIZE_PER_ADD, DEFAULT_SIZE_PER_ADD);

    for (int i = 0; i < size; ++i){
        bitmap.insert((int64_t)array[i]);
    }

    return bitmap.to_ArrayType(false);
}


/**
 * @brief construct a bitmap from the specified varbit
 *
 * @param args[0]   the varbit
 *
 * @return the bitmap representing the varbit
 */
template <typename T>
static
const ArrayType*
bitmap_from_varbit
(
    AnyType &args
){
    return Bitmap<T>(args[0].getAs<VarBit*>()).to_ArrayType(false);
}


/**
 * @brief the in function for the bitmap
 *
 * @param args[0]   the input string, which should be split by a comma.
 *
 * @return the bitmap representing the input string
 */
template <typename T>
static
const ArrayType*
bitmap_in
(
    AnyType &args
){
    return (Bitmap<T>(args[0].getAs<char*>())).to_ArrayType(false);
}


/**
 * @brief the out function for the bitmap
 *
 * @param args[0]   the bitmap
 *
 * @return the string representing the bitmap.
 */
template <typename T>
static
char*
bitmap_out
(
    AnyType &args
){
    return (Bitmap<T>(GETARG_IMMUTABLE_BITMAP(args[0]))).to_string();
}


/**
 * @brief convert the bitmap to varbit.
 *
 * @param args[0]   the bitmap
 *
 * @return the varbit representation for the bitmap.
 */
template <typename T>
static
VarBit*
bitmap_return_varbit
(
    AnyType &args
){
    return (Bitmap<T>(GETARG_IMMUTABLE_BITMAP(args[0]))).to_varbit();
}


/**
 * @brief return the internal representation (array) for the bitmap.
 *
 * @param args[0]   the bitmap
 *
 * @return the array representation for the bitmap.
 */
template <typename T>
static
const ArrayType*
bitmap_return_array
(
    AnyType &args
){
    return GETARG_IMMUTABLE_BITMAP(args[0]).array();
}


/**
 * @brief the init function for bitmap_unnest
 */
template <typename T>
static
void *
bitmap_unnest_init(AnyType &args) {
    ArrayHandle<T> arr = GETARG_IMMUTABLE_BITMAP(args[0]);
    madlib_assert(arr.size() == (size_t)arr[0],
            std::invalid_argument("invalid bitmap"));

    // allocate memory for user context
    unnest_fctx<T>*  fctx = new unnest_fctx<T>();
    fctx->bitmap = arr.ptr();
    fctx->size = arr.size();
    fctx->index = 0;
    fctx->word = 0;
    fctx->cur_pos = 0;
    fctx->max_pos = 0;

    return fctx;
}


/**
 * @brief the next function for bitmap_unnest
 *
 * @param user_fctx     the user context
 * @param is_last_call  is it the last call
 *
 * @return the current value
 */
template <typename T>
static
int64_t
bitmap_unnest_next(void *user_fctx, bool *is_last_call) {
    madlib_assert(is_last_call,
         std::invalid_argument("the paramter is_last_class should not be null"));

    unnest_fctx<T> *fctx       = static_cast<unnest_fctx<T>*>(user_fctx);
    T& curword = fctx->word;
    int& index = fctx->index;
    int64_t& cur_pos = fctx->cur_pos;
    int64_t& max_pos = fctx->max_pos;

    if (0 == curword){
        ++index;
        if (index >= fctx->size){
            *is_last_call = true;
            return -1;
        }

        // skip the composite words that represent continuous 0s
        curword = fctx->bitmap[index];
        while ((curword < 0) &&
                (0 == ((BM_WORDCNT_MASK + 1) & curword))){
            max_pos += (int64_t)(BM_WORDCNT_MASK & curword) * BM_BASE;
            ++index;
            madlib_assert(index < fctx->size,
                 std::runtime_error("invalid bitmap"));
            curword = fctx->bitmap[index];
        }
        cur_pos = max_pos;
        max_pos += curword > 0 ? 31 :
                (int64_t)(BM_WORDCNT_MASK & curword) * (int64_t)BM_BASE;
    }

    if (curword > 0){
        // the normal word has 1s
        T preword = 0;
        do{
            preword = curword;
            curword >>= 1;
            ++cur_pos;
        }while (0 == (preword & 0x01));
    }else{
        // a composite word with 1s
        ++cur_pos;
        if (cur_pos >= max_pos){
            curword = 0;
        }
    }

    *is_last_call = false;
    return cur_pos;
}


public:
/**
 * @brief the implementation for operator >
 *
 * @param args[0]   the bitmap
 * @param args[1]   the bitmap
 *
 * @return args[0] > args[1]
 *
 */
template <typename T>
static
const bool
bitmap_gt
(
    AnyType &args
){
    return bitmap_cmp_internal<T>(args) == GT;
}


/**
 * @brief the implementation for operator >=
 *
 * @param args[0]   the bitmap
 * @param args[1]   the bitmap
 *
 * @return args[0] >= args[1]
 *
 */
template <typename T>
static
const bool
bitmap_ge
(
    AnyType &args
){
    BITMAPOP op = bitmap_cmp_internal<T>(args);
    return (op == GT)  || (op == EQ);
}


/**
 * @brief the implementation for operator =
 *
 * @param args[0]   the bitmap
 * @param args[1]   the bitmap
 *
 * @return args[0] == args[1]
 *
 */
template <typename T>
static
const bool
bitmap_eq
(
    AnyType &args
){
    return bitmap_cmp_internal<T>(args) == EQ;
}


/**
 * @brief compare the two bitmap
 *
 * @param args[0]   the bitmap
 * @param args[1]   the bitmap
 *
 * @return 0 for equality; 1 for greater than; and -1 for less than
 *
 */
template <typename T>
static
const int32_t
bitmap_cmp
(
    AnyType &args
){
    return static_cast<int32_t>(bitmap_cmp_internal<T>(args));
}

protected:

template <typename T>
static
const BITMAPOP
bitmap_cmp_internal
(
    AnyType &args
){
    const T* lhs = (args[0].getAs< ArrayHandle<T> >(false)).ptr();
    const T* rhs = (args[1].getAs< ArrayHandle<T> >(false)).ptr();

    int size = (lhs[0] > rhs[0]) ? rhs[0] - 1 : lhs[0] - 1;
    int res = memcmp(lhs + 1, rhs + 1,
                     size * sizeof(T));

    if (0 == res){
        res = memcmp(lhs, rhs, sizeof(T));
    }

    return (0 == res) ? EQ : ((res < 0) ? LT : GT);
}

template <typename X>
static
int
compare(const void* lhs, const void* rhs){
    return *(X*)lhs - *(X*)rhs;
}

template <typename T>
class unnest_fctx
{
public:
    const T* bitmap;
    int size;
    int index;
    T word;
    int64_t cur_pos;
    int64_t max_pos;
};

}; // class BitmapUtil

} // namespace bitmap
} // namespace modules
} // namespace madlib

#endif //  MADLIB_MODULES_BITMAP_BITMAP_PROTO_HPP
