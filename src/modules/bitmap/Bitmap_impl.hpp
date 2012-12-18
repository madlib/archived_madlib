#ifndef MADLIB_MODULES_BITMAP_BITMAP_IMPL_HPP
#define MADLIB_MODULES_BITMAP_BITMAP_IMPL_HPP

namespace madlib {
namespace modules {
namespace bitmap {

// ctor
template <typename T>
inline
Bitmap<T>::Bitmap
(
    int capacity,
    int size_per_add
) :
    m_bmArray(NULL), m_bitmap(NULL), m_size(1), m_capacity(capacity),
    m_size_per_add(size_per_add), m_bitmap_updated(true),
    m_base(BM_BASE),
    m_wordcnt_mask(BM_WORDCNT_MASK),
    m_cw_zero_mask(BM_CW_ZERO_MASK),
    m_cw_one_mask(BM_CW_ONE_MASK){
    set_typInfo();
    m_bmArray = BM_CONSTRUCT_ARRAY((Datum*)NULL, capacity);
    m_bitmap = BM_ARR_DATA_PTR(m_bmArray, T);
    m_bitmap[0] = 1;
}

// ctor
template <typename T>
inline
Bitmap<T>::Bitmap
(
    ArrayType* arr,
    Bitmap& rhs
):
m_bmArray(arr), m_bitmap(BM_ARR_DATA_PTR(arr, T)),
m_size(BM_ARRAY_LENGTH(arr)), m_capacity(BM_ARRAY_LENGTH(arr)),
m_size_per_add(rhs.m_size_per_add), m_bitmap_updated(false),
m_base(rhs.m_base), m_wordcnt_mask(rhs.m_wordcnt_mask),
m_cw_zero_mask(rhs.m_cw_zero_mask), m_cw_one_mask(rhs.m_cw_one_mask),
m_typoid(rhs.m_typoid), m_typlen(rhs.m_typelen),
m_typbyval(rhs.m_typbyval), m_typalign(rhs.m_typalign){
BM_CHECK_ARRAY_SIZE(m_bitmap, m_size);
}


//ctor
template <typename T>
inline
Bitmap<T>::Bitmap
(
    ArrayHandle<T> handle,
    int size_per_add /* = DEFAULT_SIZE_PER_ADD */
) :
m_bmArray(const_cast<ArrayType*>(handle.array())),
m_bitmap(const_cast<T*>(handle.ptr())), m_size(handle[0]),
m_capacity(handle.size()), m_size_per_add(size_per_add),
m_bitmap_updated(false), m_base(BM_BASE),
m_wordcnt_mask(BM_WORDCNT_MASK),
m_cw_zero_mask(BM_CW_ZERO_MASK),
m_cw_one_mask(BM_CW_ONE_MASK) {
    set_typInfo();
    BM_CHECK_ARRAY_SIZE(m_bitmap, m_size);
}


//ctor
template <typename T>
inline
Bitmap<T>::Bitmap
(
    Bitmap& rhs
):
m_bmArray(rhs.m_bmArray), m_bitmap(rhs.m_bitmap), m_size(rhs.m_size),
m_capacity(rhs.m_capacity), m_size_per_add(rhs.m_size_per_add),
m_bitmap_updated(rhs.m_bitmap_updated), m_base(rhs.m_base),
m_wordcnt_mask(rhs.m_wordcnt_mask),
m_cw_zero_mask(rhs.m_cw_zero_mask),
m_cw_one_mask(rhs.m_cw_one_mask),
m_typoid(rhs.m_typoid), m_typlen(rhs.m_typelen),
m_typbyval(rhs.m_typbyval), m_typalign(rhs.m_typalign){
}


//ctor
template <typename T>
inline
Bitmap<T>::Bitmap
(
    VarBit* bits
):
m_bmArray(NULL), m_bitmap(NULL), m_size(1), m_capacity(DEFAULT_SIZE_PER_ADD),
m_size_per_add(DEFAULT_SIZE_PER_ADD), m_bitmap_updated(false),
m_base(BM_BASE),
m_wordcnt_mask(BM_WORDCNT_MASK),
m_cw_zero_mask(BM_CW_ZERO_MASK),
m_cw_one_mask(BM_CW_ONE_MASK){
    // init the member variables
    set_typInfo();
    m_bmArray = BM_CONSTRUCT_ARRAY((Datum*)NULL, m_capacity);
    m_bitmap = BM_ARR_DATA_PTR(m_bmArray, T);
    m_bitmap[0] = 1;

    bits8* pbits = VARBITS(bits);
    int64_t bitlen = VARBITLEN(bits);
    int64_t alignlen = ((bitlen + 7) >> 3) << 3;
    int64_t ignorebits = alignlen - bitlen;
    int64_t size = VARBITBYTES(bits) - 1;
    int64_t beg_bit = 1;
    int64_t cur_pos = 1;
    bits8 curbit = 0;
    for (; size >= 0; --size){
        curbit = pbits[size] >> ignorebits;
        cur_pos = beg_bit;
        do{
            if (1 == (curbit & 0x01)){
                insert(cur_pos);
            }
            ++cur_pos;
            curbit >>= 1;
        }while (curbit > 0);
        beg_bit += 8 - ignorebits;
        ignorebits = 0;
    }
}


//ctor
template <typename T>
inline
Bitmap<T>::Bitmap
(
    char* str
):
m_bmArray(NULL), m_bitmap(NULL), m_size(1), m_capacity(DEFAULT_SIZE_PER_ADD),
m_size_per_add(DEFAULT_SIZE_PER_ADD), m_bitmap_updated(false),
m_base(BM_BASE),
m_wordcnt_mask(BM_WORDCNT_MASK),
m_cw_zero_mask(BM_CW_ZERO_MASK),
m_cw_one_mask(BM_CW_ONE_MASK){
    // init the member variables
    set_typInfo();
    m_bmArray = BM_CONSTRUCT_ARRAY((Datum*)NULL, m_capacity);
    m_bitmap = BM_ARR_DATA_PTR(m_bmArray, T);
    m_bitmap[0] = 1;

    // convert the input string to a bitmap
    char elem[MAXBITSOFINT64] = {'\0'};
    int j = 0;
    int64 input_pos = 0;

    for (; *str != '\0'; ++str){
        if (',' == *str){
            elem[j] = '\0';
            (void) scanint8(elem, false, &input_pos);
            insert(input_pos);
            j = 0;
        }else{
            elem[j++] = *str;
        }
    }

    (void) scanint8(elem, false, &input_pos);
    insert(input_pos);
}


/**
 * @brief breakup the composite word, and insert the input number to it.
 *        three conditions need to be considered here (assume that the active
 *        word contains n normal words):
 *        + if the input number is inserted into the 1st word, then the active
 *          word will be breakup to:
 *              "a normal word" + "a composite word with n - 1 words"
 *        + if the input number is inserted into the nth word, then the active
 *          word will be break up to:
 *              "a composite word with n - 1 words" + "a normal word"
 *        + if the input number is inserted into ith word (1<i<n), then the
 *          active word will be breakup to:
 *              "a composite word with i - 1 words" + "a normal word" + "a composite
 *               word with n - i words
 *
 * @param newbitmap     the new bitmap for keeping the breakup bitmap. If the
 *                      old bitmap is not large enough to keep the breakup
 *                      bitmap, then we need to reallocate. Otherwise, the new
 *                      bitmap is the same as the old bitmap (in this case,
 *                      just move the elements)
 * @param index         the index of the active word will be used to insert the
 *                      input number
 * @param pos_in_word   the inserting position for the input number
 * @param word_pos      the word for the input number will
 *                      be inserted to the new bitmap
 * @param num_words     the number of words in the composite word
 *
 * @return the new bitmap.
 */
template<typename T>
void
Bitmap<T>::breakup_compword
(
    T* newbitmap,
    int32_t index,
    int32_t pos_in_word,
    int32_t word_pos,
    int32_t num_words
){
    memmove(newbitmap, m_bitmap, (index + 1) * sizeof(T));
    // the inserted position is in the middle of a composite word
    if (word_pos > 1 && word_pos < num_words){
        memmove(newbitmap + index + 2,
                m_bitmap + index, (m_size - index) * sizeof(T));
        newbitmap[index] = (T)(word_pos - 1) | m_cw_zero_mask;
        newbitmap[index + 2] = (T)(num_words - word_pos) | m_cw_zero_mask;
        ++index;
        m_size += 2;
    }else{
        memmove(newbitmap + index + 1,
                m_bitmap + index, (m_size - index) * sizeof(T));
        // the inserted position is in the beginning of a composite word
        if (1 == word_pos){
            newbitmap[index + 1] = (T)(num_words - 1) | m_cw_zero_mask;
        }else{
            // the inserted position is in the end of a composite word
            newbitmap[index] = (T)(num_words - 1) | m_cw_zero_mask;
            ++index;
        }
        m_size += 1;
    }

    newbitmap[index] = (T)1 << (pos_in_word - 1);
    m_bitmap = newbitmap;
    m_bitmap[0] = m_size;
}


/**
 * @breif insert the specified number to the composite word
 *
 * @param number        the input number
 * @param num_words     the number of words in the composite word
 * @param index         the subscript of bitmap array where the input
 *                      number will be inserted
 */
template<typename T>
void
Bitmap<T>::insert_compword
(
    int64_t number,
    int64_t num_words,
    int index
){
    int pos_in_word = get_pos_word(number);

    if (1 == num_words){
        m_bitmap[index] = (T)1 << (pos_in_word - 1);
        return;
    }

    int64_t word_pos = BM_NUMWORDS_FOR_BITS(number);
    T* newbitmap = m_bitmap;

    // need to increase the memory size
    if (((1 == word_pos || num_words == word_pos) && m_size == m_capacity) ||
        (word_pos > 1 && word_pos < num_words && m_size >= m_capacity - 1)){
            m_capacity += m_size_per_add;
            newbitmap = alloc_bitmap(m_capacity);
            m_bitmap_updated = true;
    }

    breakup_compword(newbitmap, index, pos_in_word, word_pos, num_words);
}


/**
 * @brief append the given number to the bitmap.
 *
 * @param number   the input number
 *
 * @return the new bitmap after appended.
 *
 */
template <typename T>
inline
void
Bitmap<T>::append
(
    int64_t number
){
    int64_t need_elems = 1;
    T max_bits = (T)BM_MAXBITS_IN_COMP;
    int64_t num_words = BM_NUMWORDS_FOR_BITS(number);
    int64_t cur_pos = get_pos_word(number);
    int i = m_size;

    if (num_words <= max_bits + 1){
        // a composite word can represent all zeros,
        // then two new elements are enough
        need_elems = (1 == num_words) ? 1 : 2;
    }else{
        // we need 2 more composite words to represent all zeros
        need_elems = (num_words - 1 + max_bits - 1) / max_bits + 1;
        num_words = (num_words - 1) % max_bits + 1;
    }

    if (need_elems + m_size > m_capacity){
        m_capacity += BM_ALIGN(need_elems, m_size_per_add);
        m_bitmap = alloc_bitmap(m_capacity, m_bitmap, m_size);
        m_bitmap_updated = true;
    }

    // fill the composite words
    for (; need_elems > 2; --need_elems){
        m_bitmap[i++] = m_cw_zero_mask | max_bits;
        ++m_size;
    }

    // the first word is composite word
    // the second is a normal word
    if ((2 == need_elems) && (num_words > 1)){
        m_bitmap[i] = m_cw_zero_mask | (T)(num_words - 1);
        m_bitmap[++i] = (T)1 << (cur_pos - 1);
        m_size += 2;
    }else{
        // only one normal word can represent the input number
        m_bitmap[i] = (T)1 << (cur_pos - 1);
        m_size += 1;
    }

    // set the size of the bitmap
    m_bitmap[0] = m_size;
}


/**
 * @brief if all the available bits of the normal word are 1s/0s, then
 *        we convert it to a composite word (A). Furthermore, if its previous word
 *        is composite (B) and they(A and B) have the same sign, then merge them.
 *
 * @param curword   the current word
 * @param i         the index of the current word
 *
 */
template <typename T>
inline
void
Bitmap<T>::merge_norm_to_comp
(
    T& curword,
    int i
){
    T& preword = m_bitmap[i - 1];
    // the previous word is not composite, or
    // it represents the maximum composite word for continuous one, or
    // it's a composite word with zero
    if (preword > 0 ||
        !(BM_COMPWORD_ONE(preword)) ||
        BM_COMP_ONE_MAX(preword)){
        curword = m_cw_one_mask | 1;
    }else{
        memmove(m_bitmap + i, m_bitmap + (i + 1),
                (m_size - i - 1) * sizeof(T));
        preword += 1;
        m_size -= 1;
        m_bitmap[0] = m_size;
    }
}


/**
 * @brief insert the give number to the bitmap.
 *
 * @param number   the input number
 *
 * @return the new bitmap after inserted.
 *
 * @note duplicated numbers are allowed
 *
 */
template <typename T>
inline
Bitmap<T>&
Bitmap<T>::insert
(
    int64_t number
){
    madlib_assert(number > 0,
            std::invalid_argument("the bit position must be a positive number"));

    int64_t cur_pos = 0;
    int64_t num_words = 1;
    int i = 1;

    // visit each element of the bitmap array to find the right word to
    // insert the input number
    for (i = 1; i < m_size; ++i){
        T& curword = m_bitmap[i];
        if (curword > 0){
            cur_pos += m_base;
            // insert the input bit position to a normal word
            if (cur_pos >= number){
                // use | rather than + to allow duplicated numbers insertion
                curword |= (T)1 << ((get_pos_word(number)) - 1);
                if ((~m_cw_zero_mask) == curword){
                    merge_norm_to_comp(curword, i);
                }
                return *this;
            }
        }else if (curword < 0){
            // get the number of words for a composite word
            // each word contains m_base bits
            num_words = BM_NUMWORDS_IN_COMP(curword);
            int64_t temp = num_words * m_base;
            cur_pos += temp;
            if (cur_pos >= number){
                // if the inserting position is in a composite word with 1s
                // then that's a duplicated number
                if (BM_COMPWORD_ZERO(curword)){
                    insert_compword(number - (cur_pos - temp), num_words, i);
                }
                return *this;
            }
        }
    }

    // reach the end of the bitmap
    append(number - cur_pos);

    return *this;
}


/**
 * @brief get the maximum number represented by the bitmap
 */
template <typename T>
inline
int64_t
Bitmap<T>::max_number() const{
    int64_t res = 0;
    int i = 1;
    T word = 0;
    for (; i < m_size - 1; ++i){
        word = m_bitmap[i];
        res += (word < 0 ? BM_NUMBITS_IN_COMP(word) : 31);
    }

    word = m_bitmap[i];
    if (word < 0){
        res += BM_NUMBITS_IN_COMP(word);
    }else{
        res += 31;
        T mask = m_wordcnt_mask + 1;
        while (0 == (word & mask)){
            --res;
            mask >>= 1;
        }
    }
    return res;
}


/**
 * @brief remove the specified number from the bitmap
 */
template <typename T>
inline
ArrayType*
Bitmap<T>::reset(int64_t number){
    if (number > 0 && number <= max_number()){
        return Bitmap(4,4).insert(number).op_xor(*this);
    }

    return m_bmArray;

}

/**
 * @brief bitwise operation on a normal word and a composite word
 * @param norm      the normal word
 * @param comp      the composite word
 * @param i         the index of the normal word in its bitmap
 * @param j         the index of the composite word in its bitmap
 * @param lhs       the bitmap array for the normal word
 * @param rhs       the bitmap array for the composite word
 * @param op        the function pointer for the bitwise operation
 *
 * @ rturn the result of applying 'op' on the normal word and the composite word
 */
template <typename T>
inline
T
Bitmap<T>::bitwise_norm_comp_words(T& norm, T& comp, int& i, int& j,
            T* lhs, T* rhs, bitwise_op op) const{
    T temp = (this->*op)(norm, comp);
    --comp;
    comp = BM_NUMWORDS_IN_COMP(comp) > 0 ? comp : rhs[++j];
    norm = lhs[++i];
    return temp;
}


/**
 * @brief bitwise operation on two composite words
 *
 * @param lword     the left word
 * @param rword     the right word
 * @param i         the index of the left word in its bitmap
 * @param j         the index of the right word in its bitmap
 * @param lhs       the bitmap array for the left word
 * @param rhs       the bitmap array for the right word
 *
 * @return the number of overlaped words for the two composite words
 */
template <typename T>
inline
T
Bitmap<T>::bitwise_comp_comp_words(T& lword, T& rword, int& i, int& j,
            T* lhs, T* rhs) const{
    T l_num_words = BM_NUMWORDS_IN_COMP(lword);
    T r_num_words = BM_NUMWORDS_IN_COMP(rword);
    // left composite word contains more normal words
    if (l_num_words > r_num_words){
        lword -= r_num_words;
        rword = rhs[++j];
        return r_num_words;
    }
    // right composite word contains more normal words
    if (r_num_words > l_num_words){
        rword -= l_num_words;
        lword = lhs[++i];
        return l_num_words;
    }
    // left and right composite word have the same number of normal words
    lword = lhs[++i];
    rword = rhs[++j];
    return l_num_words;
}


/**
 * @brief the entry function for doing the bitwise operations,
 *        such as |, & and ^, etc.
 *
 * @param rhs       the bitmap
 * @param op        the function used to do the real operation, such as | and &
 * @param postproc  the function used to post-processing the bitmap
 *
 * @return the result of "this" OP "rhs", where OP can be |, & and ^.
 */
template <typename T>
inline
ArrayType*
Bitmap<T>::bitwise_proc
(
    const Bitmap<T>& rhs,
    bitwise_op op,
    bitwise_postproc postproc
) const{
    int i = 1;
    int j = 1;
    int k = 1;
    T temp;
    T pre_word = 0;
    T lword = m_bitmap[i];
    T rword = rhs.m_bitmap[j];
    int capacity = m_size + rhs.m_size;
    T* result = new T[capacity];

    for (; i < m_size && j < rhs.m_size; ++k){
        // the two words have the same sign
        if ((lword ^ rword) >= 0){
            temp = (this->*op)(lword, rword);
            if (lword < 0){
                temp = (temp & m_cw_one_mask ) | bitwise_comp_comp_words
                    (lword, rword, i, j, m_bitmap, rhs.m_bitmap);
            }else{
                lword = m_bitmap[++i];
                rword = rhs.m_bitmap[++j];
            }
        }else{
            temp = lword > 0 ?
                bitwise_norm_comp_words
                    (lword, rword, i, j, m_bitmap, rhs.m_bitmap, op) :
                bitwise_norm_comp_words
                    (rword, lword, j, i, rhs.m_bitmap, m_bitmap, op);
        }

        // merge if needed
        if (k >= 2 && BM_SAME_SIGN(temp, pre_word)){
            pre_word += BM_NUMWORDS_IN_COMP(temp);
            result[--k] = pre_word;
        }else{
            result[k] = temp;
            pre_word = temp;
        }
    }
    // post-processing
    k = (this->*postproc)(result, k, *this, i, lword, pre_word);
    k = (this->*postproc)(result, k, rhs, j, rword, pre_word);

    // if the bitmap only has one word, and the word is a composite word with all
    // values are 0, then trim it
    k = (2 == k) && ((pre_word & m_cw_one_mask) == m_cw_zero_mask) ? 1 : k;

    madlib_assert(k <= capacity,
        std::logic_error
        ("the real size of the bitmap should be no greater than its capacity"));

    result[0] = (T)k;

    if (1 == k){
        return NULL;
    }

    ArrayType* arr = alloc_array(result, k);
    delete[] result;
    return arr;
}


/**
 * @brief the bitwise xor operation on two words
 *
 * @param lhs   the left word
 * @param rhs   the right word
 *
 * @return return the xor result if one of the two words is normal word
 *         return the sign of xor result if the two words are composite.
 */
template <typename T>
inline
T
Bitmap<T>::bitwise_xor
(
    T lhs,
    T rhs
) const{
    T res = 0;
    if (lhs > 0 && rhs > 0){
        res = (lhs ^ rhs) & (~m_cw_zero_mask);
        if (0 == res){
            res = m_cw_zero_mask | 1;
        }else if ((~m_cw_zero_mask) == res){
            res = m_cw_one_mask | 1;
        }
    }else if (lhs < 0 && rhs > 0){
        res = BM_COMPWORD_ONE(lhs) ? (~rhs) & (~m_cw_zero_mask) : rhs;
    }else if (lhs > 0 && rhs < 0){
        res = BM_COMPWORD_ONE(rhs) ? (~lhs) & (~m_cw_zero_mask) : lhs;
    }else{
        res = (0 == ((lhs ^ rhs) & (m_wordcnt_mask + 1))) ?
                m_cw_zero_mask : m_cw_one_mask;
    }

    return res;
}


/**
 * @brief the post-processing for the XOR operation. Here, we need to concat
 *        the remainder bitmap elements to the result
 *
 * @param result    the array for keeping the 'or' result of two bitmaps
 * @param k         the subscript if we insert new element to the result array
 * @param bitmap    the bitmap that need to merge to the result array
 * @param i         the index of the current word in the bitmap array
 * @param curword   the current processing word in the bitmap array
 * @param pre_word  the previous word in the bitmap array, compared with curword
 *
 * @return the number of elements in the result array
 */
template <typename T>
inline
int
Bitmap<T>::xor_postproc
(
    T* result,
    int k,
    const Bitmap<T>& bitmap,
    int i,
    T curword,
    T pre_word
) const{
    // value | 0 == value ^ 0;
    return or_postproc(result, k, bitmap, i, curword, pre_word);
}


/**
 * @brief implement the operator ^
 *
 * @param rhs   the bitmap
 *
 * @return the result of "this" ^ "rhs"
 *
 */
template <typename T>
inline
ArrayType*
Bitmap<T>::op_xor (const Bitmap<T>& rhs) const{
    return bitwise_proc(rhs, &Bitmap<T>::bitwise_xor, &Bitmap<T>::xor_postproc);
}


/**
 * @brief override the operator ^
 *
 * @param rhs   the bitmap
 *
 * @return the result of "this" ^ "rhs"
 *
 */
template <typename T>
inline
Bitmap<T>
Bitmap<T>::operator ^ (const Bitmap<T>& rhs) const{
    ArrayType* res = bitwise_proc
            (rhs, &Bitmap<T>::bitwise_xor, &Bitmap<T>::xor_postproc);
    return res ? Bitmap(res, *this) : EMTYP_BITMAP;
}


/**
 * @brief the bitwise or operation on two words.
 *
 * @param lhs   the left word
 * @param rhs   the right word
 *
 * @return lhs | rhs if one of the two words is a normal word.
 *         if both of them are composite words, return the sign of lhs | rhs.
 */
template <typename T>
inline
T
Bitmap<T>::bitwise_or
(
    T lhs,
    T rhs
)  const{
    T res = 0;
    if ((lhs ^ rhs) >= 0){
        res = lhs | rhs;
    }else if (lhs < 0){
        res = BM_COMPWORD_ONE(lhs) ? m_cw_one_mask | 1: rhs;
    }else {
        res = BM_COMPWORD_ONE(rhs) ? m_cw_one_mask | 1: lhs;
    }

    // if all the bits of the result are 1, then use a composite word
    // to represent it
    return res == (~m_cw_zero_mask) ? m_cw_one_mask | 1 : res;
}


/**
 * @brief the post-processing for the OR operation. Here, we need to concat
 *        the remainder bitmap elements to the result
 *
 * @param result    the array for keeping the 'or' result of two bitmaps
 * @param k         the subscript if we insert new element to the result array
 * @param bitmap    the bitmap that need to merge to the result array
 * @param i         the index of the current word in the bitmap array
 * @param curword   the current processing word in the bitmap array
 * @param pre_word  the previous word in the bitmap array, compared with curword
 *
 * @return the number of elements in the result array
 */
template <typename T>
inline
int
Bitmap<T>::or_postproc
(
    T* result,
    int k,
    const Bitmap<T>& bitmap,
    int i,
    T curword,
    T pre_word
)  const{
    for (; i < bitmap.m_size; ++k){
        T temp = (curword < 0) ? curword :
                    ((curword == (~m_cw_zero_mask)) ?
                    (m_cw_one_mask | 1) : curword);
        if (k >= 2 && BM_SAME_SIGN(temp, pre_word)){
            int64_t n1 = BM_NUMWORDS_IN_COMP(temp);
            int64_t n2 = BM_NUMWORDS_IN_COMP(pre_word);
            if (n1 + n2 > (int64_t)m_wordcnt_mask){
                result[k - 1] = (pre_word & m_cw_one_mask) | m_wordcnt_mask;
                pre_word = (pre_word & m_cw_one_mask) | (n1 + n2 - m_wordcnt_mask);
                result[k] = pre_word;
            }else{
                pre_word += n1;
                result[--k] = pre_word;
            }
        }else{
            result[k] = curword;
            pre_word = curword;
        }
        curword = bitmap.m_bitmap[++i];
    }

    return k;
}


/**
 * @brief implement the operator |
 *
 * @param rhs   the bitmap
 *
 * @return the result of "this" | "rhs"
 *
 */
template <typename T>
inline
ArrayType*
Bitmap<T>::op_or (const Bitmap<T>& rhs) const{
    return bitwise_proc(rhs, &Bitmap<T>::bitwise_or, &Bitmap<T>::or_postproc);
}


/**
 * @brief override the operator |
 *
 * @param rhs   the bitmap
 *
 * @return the result of "this" | "rhs"
 *
 */
template <typename T>
inline
Bitmap<T>
Bitmap<T>::operator | (const Bitmap<T>& rhs) const{
    ArrayType* res = bitwise_proc
            (rhs, &Bitmap<T>::bitwise_or, &Bitmap<T>::or_postproc);
    return res ? Bitmap(res, *this) : EMTYP_BITMAP;
}


/**
 * @brief the bitwise and operation on two words
 *
 * @param lhs   the left word
 * @param rhs   the right word
 *
 * @return lhs & rhs
 */
template <typename T>
inline
T
Bitmap<T>::bitwise_and
(
    T lhs,
    T rhs
) const{
    T res = 0;
    if ((lhs ^ rhs) >= 0){
        res = lhs & rhs;
    }else if (lhs < 0 && rhs > 0){
        res = BM_COMPWORD_ONE(lhs) ? rhs : m_cw_zero_mask | 1;
    }else {
        res = BM_COMPWORD_ONE(rhs) ? lhs : m_cw_zero_mask | 1;
    }

    // if all the bits of the result are 0, then use a composite word
    // to represent it
    return (0 == res) ? (m_cw_zero_mask | 1) : res;
}


/**
 * @brief implement the operator &
 *
 * @param rhs   the bitmap
 *
 * @return the result of "this" & "rhs"
 *
 */
template <typename T>
inline
ArrayType*
Bitmap<T>::op_and (const Bitmap<T>& rhs) const{
    return bitwise_proc(rhs, &Bitmap<T>::bitwise_and, &Bitmap<T>::and_postproc);
}


/**
 * @brief override the operator &
 *
 * @param rhs   the bitmap
 *
 * @return the result of "this" & "rhs"
 *
 */
template <typename T>
inline
Bitmap<T>
Bitmap<T>::operator & (const Bitmap<T>& rhs) const{
    ArrayType* res = bitwise_proc
            (rhs, &Bitmap<T>::bitwise_and, &Bitmap<T>::and_postproc);
    return res ? Bitmap(res, *this) : EMTYP_BITMAP;
}


/**
 * @brief implement the operator ~
 *
 * @return the result of ~"this"
 *
 */
template <typename T>
inline
ArrayType*
Bitmap<T>::op_not () const{
    T* result = new T[m_size];
    T curword = 0;
    int k = 1;
    for (int i = 1; i < m_size; ++i){
        curword = m_bitmap[i];
        result[i] = (curword > 0) ?  (~m_cw_zero_mask) & (~curword) :
                        BM_COMPWORD_SWAP(curword);
    }

    k = m_size - 1;
    // If the highest bits of the last word are zeros, then those bits should
    // not be reverted, since they are zeros without representing any numbers
    // For example, ~0x00000FFF should be 0x00000000, and
    // ~0x000030F0 should be 0x00000F0F
    if (curword > 0){
        if (1 == get_nonzero_cnt(curword + 1)){
            --k;
        }else{
            T& lastword = result[k];
            T mask = BM_CW_ONE_MASK ^ BM_CW_ZERO_MASK;
            while ((lastword & mask) > 0){
                lastword ^= mask;
                mask >>= 1;
            }
        }
    }
    // trim the word with 0s
    while((k > 0) && (result[k] < 0) && BM_COMPWORD_ZERO(result[k])){
        --k;
    }
    if (0 == k)
        return NULL;

    result[0] = k + 1;
    ArrayType* arr = alloc_array(result, k + 1);
    delete[] result;
    return arr;
}


/**
 * @brief override the operator ~
 *
 * @param rhs   the bitmap
 *
 * @return the result of ~"this"
 *
 */
template <typename T>
inline
Bitmap<T> Bitmap<T>::operator ~() const{
    return Bitmap(op_not(), *this);
}


template <typename T>
inline
int32_t Bitmap<T>::operator [](size_t index) const{
    int64_t curpos = 0;
    int64_t numbits = 0;
    for (int i = 1; i < m_size; ++i){
        T word = m_bitmap[i];
         numbits = (word > 0 ? 31 : BM_NUMBITS_IN_COMP(word));
         curpos += numbits;
        if (index <= curpos){
            return BM_BIT_TEST(word, index - (curpos - numbits));
        }
    }
    return 0;
}
/**
 * @brief get the number of nonzero bits
 *
 * @return the nonzero count
 */
template <typename T>
inline
int64_t
Bitmap<T>::nonzero_count() const{
    int64_t   res = 0;
    for (int i = 1; i < m_size; ++i){
        if (m_bitmap[i] > 0){
            res += get_nonzero_cnt(m_bitmap[i]);
        }else{
            // this is a composite word
            res += (0 == (m_bitmap[i] & (m_wordcnt_mask + 1))) ?
                    0 : (m_bitmap[i] & m_wordcnt_mask) * m_base;
        }
    }

    return res;
}


/**
 * @brief get the positions of the non-zero bits. The position starts from 1.
 *
 * @param result    the array used to keep the positions
 *
 * @return the array containing the positions whose bits are 1.
 */
template <typename T>
inline
int64_t
Bitmap<T>::nonzero_positions(int64_t* result) const{
    madlib_assert(result != NULL,
            std::invalid_argument("the positions array must not be NULL"));
    int64_t j = 0;
    int64_t k = 1;
    int64_t begin_pos = 1;
    for (int i = 1; i < m_size; ++i){
        k = begin_pos;
        T word = m_bitmap[i];
        if (word > 0){
            do{
                if (1 == (word & 0x01))
                    result[j++] = k;
                word >>= 1;
                ++k;
            }while (word > 0);
            begin_pos += m_base;
        }else{
            if ((word & (m_wordcnt_mask + 1)) > 0){
                int64_t n = (word & m_wordcnt_mask) * (int64_t)m_base;
                for (; n > 0 ; --n){
                    result[j++] = k++;
                }
            }
            begin_pos += BM_NUMBITS_IN_COMP(word);
        }
    }

    return j;
}


/**
 * @brief get the positions of the non-zero bits. The position starts from 1.
 *
 * @return the array contains the positions
 *
 */
template <typename T>
inline
ArrayType*
Bitmap<T>::nonzero_positions() const{
    int64_t* result = NULL;
    int size = nonzero_count();
    ArrayType* res_arr = alloc_array<int64_t>(result, size);
    nonzero_positions(result);

    return res_arr;
}


/**
 * @brief allocate a new space for the string, the old space will be free
 *
 * @param oldstr    the old string
 * @param len       the length of the old string
 */
template <typename T>
inline
char*
Bitmap<T>::to_string_realloc
(
    char* oldstr,
    int64_t& len
) const{
    // here, we shouldn't align the size, mustn't use BM_ALIGN_ALLOC0
    len += MAXBITSOFINT64 * 16;
    char* newstr = (char*)BM_ALLOC0(len* sizeof(char));
    if (NULL != oldstr){
        memcpy(newstr, oldstr, len * sizeof(char));
        BM_FREE0(oldstr);
        oldstr = NULL;
    }

    return newstr;
}


/**
 * @brief construct a string for a series
 *
 * @param pstr      the string used to keep the input numbers
 * @param totallen  the total length of the pstr
 * @param curlen    the current length of the pstr
 * @param cont_beg  the begin number for the series
 * @param cont_cnt  the count of numbers in the series.
 *
 */
template <typename T>
inline
char*
Bitmap<T>::to_string_internal
(
    char* pstr,
    int64_t& totallen,
    int64_t& curlen,
    int64_t& cont_beg,
    int64_t& cont_cnt
) const{
    if (0 == cont_cnt)
        return pstr;

    int64_t len_beg = 0;
    int64_t len_end = 0;
    char str_beg[MAXBITSOFINT64 + 1] = {'\0'};
    char str_end[MAXBITSOFINT64 + 1] = {'\0'};

    if (cont_cnt >= 2){
        len_beg = int64_to_string(str_beg, cont_beg);
        len_end = int64_to_string(str_end, cont_beg + cont_cnt - 1);
        if (len_beg + len_end + curlen + 3 > totallen){
            pstr = to_string_realloc(pstr, curlen);
        }
        memcpy(pstr + curlen, str_beg, len_beg * sizeof(char));
        curlen += len_beg;
        *(pstr + curlen) = (2 == cont_cnt) ? ',' : '~';
        ++curlen;
        memcpy(pstr + curlen, str_end, len_end * sizeof(char));
        curlen += len_end;
    }else{
        len_beg = int64_to_string(str_beg, cont_beg);
        if (len_beg + curlen + 2 > totallen){
            pstr = to_string_realloc(pstr, curlen);
        }
        memcpy(pstr + curlen, str_beg, len_beg * sizeof(char));
        curlen += len_beg;
    }

    *(pstr + curlen) = ',';
    ++curlen;
    *(pstr + curlen) = '\0';
    cont_cnt = 0;
    cont_beg = 0;

    return pstr;
}


/**
 * @brief convert the bitmap to a readable format.
 *        If more than 2 elements are continuous, then we use '~' to concat
 *        the begin and the end of the continuous numbers. Otherwise, we will
 *        use ',' to concat them.
 *        e.g. assume the bitmap is '1,2,3,5,6,8,10,11,12,13'::bitmap, then
 *        the output of to_string is '1~3,5,6,8,10~13'
 *
 * @return the readable string representation for the bitmap.
 */
template <typename T>
inline
char*
Bitmap<T>::to_string() const{
    int64_t k = 1;
    int64_t begin_pos = 1;
    int64_t cont_cnt = 0;
    int64_t cont_begin = 0;
    int64_t totallen = 0;
    int64_t curlen = 0;
    char* res = to_string_realloc(NULL, totallen);
    for (int i = 1; i < m_size; ++i){
        k = begin_pos;
        T word = m_bitmap[i];
        // a normal word
        if (word > 0){
            do{
                if (1 == (word & 0x01)){
                    ++cont_cnt;
                    cont_begin = cont_begin > 0 ? cont_begin : k;
                }else{
                    res = to_string_internal
                        (res, totallen, curlen, cont_begin, cont_cnt);
                }
                word >>= 1;
                ++k;
            }while (word > 0);

            if (0 == (m_bitmap[i] & (m_wordcnt_mask + 1))){
                res = to_string_internal
                    (res, totallen, curlen, cont_begin, cont_cnt);
            }
            begin_pos += m_base;
        }else{
            int64_t numbits = BM_NUMBITS_IN_COMP(word);
            if (BM_COMPWORD_ONE(word)){
                cont_begin = cont_begin > 0 ? cont_begin : k;
                cont_cnt += numbits;
            }else{
                res = to_string_internal
                        (res, totallen, curlen, cont_begin, cont_cnt);
            }
            begin_pos += numbits;
        }
    }

    if (cont_cnt > 0){
        res = to_string_internal
                (res, totallen, curlen, cont_begin, cont_cnt);
    }

    *(res + curlen - 1) = '\0';

    return res;
}


/**
 * @brief convert the bitmap to varbit
 *
 * @return the varbit representation for the bitmap
 */
template <typename T>
inline
VarBit*
Bitmap<T>::to_varbit() const{
    int64_t size = nonzero_count();
    int64_t* pos = new int64_t[size];
    nonzero_positions(pos);

    // get the varbit related information
    int64_t bitlen = pos[size - 1];
    int64_t alignlen = ((bitlen + 7) >> 3) << 3;
    int64_t ignorebits = alignlen - bitlen;
    int64_t len = VARBITTOTALLEN(bitlen);
    VarBit* result = (VarBit*)BM_ALIGN_ALLOC0(len);
    SET_VARSIZE(result, len);
    VARBITLEN(result) = bitlen;
    bits8* pres = VARBITS(result);
    int64_t arrlen = (alignlen >> 3);

    // set the varbit
    for (int i = 0; i < size; ++i){
        int64_t curpos = pos[i] + ignorebits;
        int64_t curindex = arrlen - ((curpos + 7) >> 3);
        curpos &= 0x07;
        curpos = (0 == curpos) ? 7 : curpos - 1;
        *(pres + curindex) += ((bits8)1 << curpos);
    }
    return result;
}


/**
 * @brief transform the bitmap to an ArrayHandle instance
 *
 * @param use_capacity  true if we don't trim the 0 elements
 *
 * @param the ArrayHandle instance for the given bitmap.
 */
template <typename T>
inline
ArrayType*
Bitmap<T>::to_ArrayType
(
    bool use_capacity /* = true */
) const{
    if (use_capacity || (m_size == m_capacity))
        return m_bmArray;

    if (empty())
        return NULL;

    // do not change m_bitmap and m_bmArray
    return alloc_array(m_bitmap, m_size);
}

} // namespace bitmap
} // namespace modules
} // namespace madlib

#endif
