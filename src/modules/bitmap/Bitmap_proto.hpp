#ifndef MADLIB_MODULES_BITMAP_BITMAP_PROTO_HPP
#define MADLIB_MODULES_BITMAP_BITMAP_PROTO_HPP

#include <dbconnector/dbconnector.hpp>


namespace madlib {
namespace modules {
namespace bitmap {

using madlib::dbconnector::postgres::madlib_get_typlenbyvalalign;

// the public interfaces for bitmap
#define RETURN_BITMAP_INTERNAL(val, T) \
            return AnyType(ArrayHandle<T>(val), false, false)
#define RETURN_BITMAP_NULL_INTERNAL(val, T) \
            const ArrayType* res = val; \
            return NULL != res ? AnyType(ArrayHandle<T>(res), false, false) : AnyType()
#define RETURN_ARRAY(val, T) \
            return AnyType(ArrayHandle<T>(val), false, false)
#define RETURN_BASE(val) \
            return AnyType(val)
#define GETARG_MUTABLE_BITMAP_INTERNAL(arg, T) \
            ((arg).getAs< MutableArrayHandle<T> >(false, false))
#define GETARG_CLONED_BITMAP_INTERNAL(arg, T) \
            ((arg).getAs< MutableArrayHandle<T> >(false, true))
#define GETARG_IMMUTABLE_BITMAP_INTERNAL(arg, T) \
            ((arg).getAs< ArrayHandle<T> >(false, false))

// return
#define RETURN_BITMAP(val)             RETURN_BITMAP_INTERNAL(val, int32_t)
#define RETURN_BITMAP_NULL(val)        RETURN_BITMAP_NULL_INTERNAL(val, int32_t)
#define RETURN_INT4_ARRAY(val)         RETURN_ARRAY(val, int32_t)
#define RETURN_INT8_ARRAY(val)         RETURN_ARRAY(val, int64_t)

// get arguments
#define GETARG_MUTABLE_BITMAP(arg)     GETARG_MUTABLE_BITMAP_INTERNAL(arg, int32_t)
#define GETARG_CLONED_BITMAP(arg)      GETARG_CLONED_BITMAP_INTERNAL(arg, int32_t)
#define GETARG_IMMUTABLE_BITMAP(arg)   GETARG_IMMUTABLE_BITMAP_INTERNAL(arg, int32_t)

// internal defines
#define INT64FORMAT     "%lld"
#define MAXBITSOFINT64  25
#define BYTESIZE        8
// the nonzero positions array may be extremely large
// here we limit it to 32MB
#define BM_MAX_BITS     (1<<25)

#define DEFAULT_SIZE_PER_ADD    16
#define EMTYP_BITMAP            Bitmap(1, DEFAULT_SIZE_PER_ADD)

#define BM_BASE             (sizeof(T) * BYTESIZE - 1)
#define BM_WORDCNT_MASK     (((T)1 << (BM_BASE - 1)) - 1)
#define BM_CW_ZERO_MASK     ((T)1 << BM_BASE)
#define BM_CW_ONE_MASK      ((T)3 << (BM_BASE - 1))

// the following macros will be used to calculate the number of 1s in an integer
#define BM_POW(c, T)        ((T)1<<(c))
#define BM_MASK(c, T)       (((T)-1) / (BM_POW(BM_POW(c, T), T) + 1))
#define BM_ROUND(n, c, T)   (((n) & BM_MASK(c, T)) + ((n) >> BM_POW(c, T) & BM_MASK(c, T)))

// align the 'val' with 'align' length
#define BM_ALIGN(val, align)    ((((val) + (align) - 1) / (align)) * (align))

// does the composite word represent continuous 1s
#define BM_COMPWORD_ONE(val)    (((val) & (m_wordcnt_mask + 1)) > 0)
// does the composite word represent continuous 0s
#define BM_COMPWORD_ZERO(val)   (((val) & (m_wordcnt_mask + 1)) == 0)
// change a composite word with 1s/0s to a composite word with 0s/1s
#define BM_COMPWORD_SWAP(val) \
    BM_NUMWORDS_IN_COMP(val) | (((val) & m_cw_one_mask) ^ (m_wordcnt_mask + 1))

// the two composite words are the same sign?
#define BM_SAME_SIGN(lhs, rhs) (((lhs) < 0) && ((rhs) < 0) && \
                               (0 == (((lhs) ^ (rhs)) & (m_wordcnt_mask + 1))))

// get the number of words for representing the input number
#define BM_NUMWORDS_FOR_BITS(val)   (((val) + m_base - 1) / m_base)
// get the number of words in the composite word
#define BM_NUMWORDS_IN_COMP(val)    ((val) & m_wordcnt_mask)
#define BM_NUMBITS_IN_COMP(val)     BM_NUMWORDS_IN_COMP(val) * (int64_t)m_base
#define BM_BIT_TEST(val, bit)  \
         ((val) > 0 ?  (((val) & (1 << ((bit) - 1))) > 0) : BM_COMPWORD_ONE(val))

// the maximum value for a composite word with continuous 1s/0s
#define BM_COMP_ONE_MAX(val)   (val == ((T)-1))
#define BM_COMP_ZERO_MAX(val)  (val == (m_wordcnt_mask | m_cw_zero_mask))

// get the maximum 0s or 1s can be represented by a composite word
// for bitmap4, its 0x3F FF FF FF.
#define BM_MAXBITS_IN_COMP ((int64_t)1 << (m_base - 1)) - 1

// the wrapper for palloc0, this function will not align the 'size'
#define BM_ALLOC0(size) palloc0(size)
#define BM_FREE0(ptr)   pfree(ptr);

// the madlib allocation function will align the 'size' to 16
#define BM_ALIGN_ALLOC0(size) madlib::defaultAllocator().allocate<\
        madlib::dbal::FunctionContext, \
        madlib::dbal::DoZero, \
        madlib::dbal::ThrowBadAlloc>(size)

// wrapper definition for ArrayType related functions
#define BM_ARR_DATA_PTR(val, T) reinterpret_cast<T*>(ARR_DATA_PTR(val))
#define BM_ARRAY_LENGTH(val) ArrayGetNItems(ARR_NDIM(val), ARR_DIMS(val))
#define BM_CONSTRUCT_ARRAY(result, size) \
    construct_array( result, size, m_typoid, m_typlen, m_typbyval, m_typalign);
#define BM_CONSTRUCT_ARRAY_TYPE(result, size, typoid, typlen, typbyval, typalign) \
    construct_array( result, size, typoid, typlen, typbyval, typalign);

#define BM_CHECK_ARRAY_SIZE(array, size)     \
        madlib_assert((NULL != (array)) && (size == (array)[0]), \
        std::invalid_argument("invalid bitmap"));

/**
 * the class for building and manipulating the bitmap.
 * We use an array as the underlying implementation for the bitmap. The first
 * element in the array keeps the real size of that array. The reasons why we
 * keep the real size of the array are:
 *  1) As the capacity of the array is always greater than the real size,
 *  we can get the real size of the array using the first element rather than
 *  traverse the whole array to get the real size;
 *  2) one addition element is small compared to the whole Array.
 *
 * the bitmap array can be dynamically reallocated. If the required size of the
 * bitmap is greater than the capacity of the bitmap, then we will allocate a
 * new memory for the new bitmap and copy the old bitmap elements to the new one.
 * And we define a parameter "size_per_add" to indicate the number of elements
 * will be added to the bitmap array.
 *
 */
// T* is the type of the underlying storage for the bitmap
template <typename T>
class Bitmap{
    // function pointers for bitwise operation
    typedef T (Bitmap<T>::*bitwise_op)(T, T) const;
    typedef int (Bitmap<T>::*bitwise_postproc)(T*, int, const Bitmap<T>&, int, T, T) const;

public:
    //ctor
    Bitmap(int capacity, int size_per_add);

    //ctor
    Bitmap(ArrayType* arr, Bitmap& rhs);

    //ctor
    Bitmap(ArrayHandle<T> handle, int size_per_add = DEFAULT_SIZE_PER_ADD);

    //ctor
    Bitmap(Bitmap& rhs);

    // construct a bitmap from the input string
    // the format of input string looks like "1,3,19,20".
    // we use comma to separate the numbers
    Bitmap(char* rhs);

    // construct a bitmap from the varbit
    Bitmap(VarBit* bits);

    // if the bitmap was reallocated, the flag will be set to true
    inline bool updated() const{
        return m_bitmap_updated;
    }

    // is the bitmap full
    inline bool full() const{
        return m_size == m_capacity;
    }

    inline bool empty() const{
        return 1 == m_size;
    }

    // insert the specified number to the bitmap
    inline Bitmap& insert(int64_t number);

    // set the specified bit to 0
    inline ArrayType* reset(int64_t number);

    // transform the bitmap to an ArrayType* instance
    inline ArrayType* to_ArrayType(bool use_capacity = true) const;

    // override bitwise operators
    inline Bitmap operator | (const Bitmap& rhs) const;
    inline Bitmap operator & (const Bitmap& rhs) const;
    inline Bitmap operator ~() const;
    inline Bitmap operator ^(const Bitmap& rhs) const;

    // override operator []
    inline int32_t operator [](size_t index) const;

    // to avoid the overhead of constructing bitmap object and ArrayHandle object,
    // the functions start with op_ will return ArrayType*
    inline ArrayType* op_or(const Bitmap& rhs) const;
    inline ArrayType* op_and(const Bitmap& rhs) const;
    inline ArrayType* op_xor(const Bitmap& rhs) const;
    inline ArrayType* op_not() const;

    // convert the bitmap to a readable format
    inline char* to_string() const;

    // convert the bitmap to varbit
    inline VarBit* to_varbit() const;

    // get the positions of the non-zero bits. The position starts from 1
    inline ArrayType* nonzero_positions() const;

    // get the number of nonzero bits
    inline int64_t nonzero_count() const;

protected:
    // insert the specified number to the composite word
    inline void insert_compword(int64_t number, int64_t num_words, int index);

    // breakup the composite word, and insert the number to it
    inline void breakup_compword (T* newbitmap, int index,
            int pos_in_word, int word_pos, int num_words);

    // append a number to the bitmap
    inline void append(int64_t number);

    // merge a normal word to a composite word
    inline void merge_norm_to_comp(T& curword, int index);

    // the entry function for doing the bitwise operations,
    // such as | and &, etc.
    inline ArrayType* bitwise_proc(const Bitmap<T>& rhs, bitwise_op op,
            bitwise_postproc postproc) const;

    // bitwise operation on a normal word and a composite word
    inline T bitwise_norm_comp_words(T& norm, T& comp, int& i, int& j,
                    T* lhs, T* rhs, bitwise_op op) const;

    // bitwise operation on two composite words
    inline T bitwise_comp_comp_words(T& lword, T& rword, int& i, int& j,
                    T* lhs, T* rhs) const;

    // lhs | rhs. If both lhs and rhs are composite words, then return
    // the sign of the result
    inline T bitwise_or(T lhs, T rhs) const;
    // the post-processing for the OR operation. the remainder bitmap elements
    // will do the | operation with zero
    inline int or_postproc(T* result, int k, const Bitmap<T>& bitmap,
                            int i, T curword, T pre_word) const;

    // lhs & rhs. If both lhs and rhs are composite words, then return
    // the sign of the result
    inline T bitwise_and(T lhs, T rhs) const;
    // the post-processing for the AND operation. Here, we do nothing,
    // since the result of any words & zero is zero.
    inline int and_postproc(T*, int k, const Bitmap<T>&, int, T, T) const{
        return k;
    }

    // lhs ^ rhs. If both lhs and rhs are composite words, then return
    // the sign of the result
    inline T bitwise_xor(T lhs, T rhs) const;
    // the post-processing for the AND operation. the remainder bitmap elements
    // will do the ^ operation with zero
    inline int xor_postproc(T*, int k, const Bitmap<T>&, int, T, T) const;

    // get the number of 1s
    inline int64_t get_nonzero_cnt(int32_t value) const{
        uint32_t res = value;
        res = BM_ROUND(res, 0, uint32_t);
        res = BM_ROUND(res, 1, uint32_t);
        res = BM_ROUND(res, 2, uint32_t);
        res = BM_ROUND(res, 3, uint32_t);
        res = BM_ROUND(res, 4, uint32);

        return static_cast<int64_t>(res);
    }

    // get the number of 1s
    inline int64_t get_nonzero_cnt(int64_t value) const{
        uint64 res = value;
        res = BM_ROUND(res, 0, uint64);
        res = BM_ROUND(res, 1, uint64);
        res = BM_ROUND(res, 2, uint64);
        res = BM_ROUND(res, 3, uint64);
        res = BM_ROUND(res, 4, uint64);
        res = BM_ROUND(res, 5, uint64);

        return static_cast<T>(res);
    }

    // get an array containing the positions of the nonzero bits
    // the input parameter should not be null
    inline int64_t nonzero_positions(int64_t* result) const;

    // get an array containing the positions of the nonzero bits
    // the function will allocate memory for holding the positions
    // the size of the array is passed by reference, the returned value
    // is the nonzero positions
    inline int64_t* nonzero_positions(int64_t& size) const;

    // retrieve the max number stored in the bitmap
    inline int64_t max_number() const;

    // allocate a new array with ArrayType encapsulated
    // X is the type of the array, we need to search the type
    // related information from cache.
    template <typename X>
    inline ArrayType* alloc_array(X*& res, int size) const{
        Oid typoid = get_Oid((X)0);
        int16 typlen;
        bool typbyval;
        char typalign;

        madlib_get_typlenbyvalalign
                    (typoid, &typlen, &typbyval, &typalign);

        ArrayType* arr = BM_CONSTRUCT_ARRAY_TYPE(
                (Datum*)NULL, size, typoid, typlen, typbyval, typalign);
        res = BM_ARR_DATA_PTR(arr, X);

        return arr;
    }

    // allocate an array to keep the elements in 'oldarr'
    // the number of those elements is 'size'. The type of
    // the array is T.
    inline ArrayType* alloc_array(T* oldarr, int size) const{
        ArrayType* res = BM_CONSTRUCT_ARRAY((Datum*)NULL, size);
        memcpy(BM_ARR_DATA_PTR(res, T), oldarr, size * sizeof(T));

        return res;
    }

    // allocate a new bitmap, all elements are zeros
    inline T* alloc_bitmap(int size){
        m_bmArray = BM_CONSTRUCT_ARRAY((Datum*)NULL, size);
        return BM_ARR_DATA_PTR(m_bmArray, T);
    }

    // allocate a new bitmap, and copy the oldbitmap to the new bitmap
    inline T* alloc_bitmap(int newsize, T* oldbitmap, int oldsize){
        T* result = alloc_bitmap(newsize);
        memcpy(result, oldbitmap, oldsize * sizeof(T));

        return result;
    }


    // get the inserting position for the input number.
    // The result is in range of [1, m_base]
    inline int get_pos_word(int64_t number) const{
        int pos = number % m_base;
        return 0 == pos  ? m_base : pos;
    }

    // transform the specified value to a Datum
    inline Datum get_Datum(int32_t elem) const{
        return Int32GetDatum(elem);
    }

    // transform the specified value to a Datum
    inline Datum get_Datum(int64_t elem) const{
        return Int64GetDatum(elem);
    }

    // get the OID for int32
    inline Oid get_Oid(int32_t) const{
        return INT4OID;
    }

    // get the OID for int64_t
    inline Oid get_Oid(int64_t) const{
        return INT8OID;
    }

    // set the type related information to the member variables
    inline void set_typInfo(){
        m_typoid = get_Oid((T)0);
        madlib_get_typlenbyvalalign
            (m_typoid, &m_typlen, &m_typbyval, &m_typalign);
    }

    char* to_string_realloc(char* oldstr, int64_t& len) const;
    char* to_string_internal(char* pstr, int64_t& totallen, int64_t& curlen,
            int64_t& cont_beg, int64_t& cont_cnt) const;

    // convert int64 number to a string
    int int64_to_string (char* result, int64_t value) const{
        int len = 0;
        if ((len = snprintf(result, MAXBITSOFINT64, INT64FORMAT, value)) < 0)
            throw std::invalid_argument("the input value is not int8 number");
        result[len] = '\0';

        return len;
    }

private:
    // the bitmap array related info
    ArrayType* m_bmArray;
    T* m_bitmap;
    int m_size;
    int m_capacity;
    int m_size_per_add;
    bool m_bitmap_updated;

    // const variables
    const int m_base;
    const T m_wordcnt_mask;
    const T m_cw_zero_mask;
    const T m_cw_one_mask;

    // type information
    Oid m_typoid;
    int16 m_typlen;
    bool m_typbyval;
    char m_typalign;

}; // class bitmap

} // namespace bitmap
} // namespace modules
} // namespace madlib

#endif
