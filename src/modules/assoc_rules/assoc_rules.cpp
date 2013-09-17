/* ----------------------------------------------------------------------- *//**
 *
 * @file assoc_rules.cpp
 *
 * @brief Functions for Apriori algorithm in MADlib
 *
 * @date August 1, 2012
 *//* ----------------------------------------------------------------------- */


#include <dbconnector/dbconnector.hpp>

#include "assoc_rules.hpp"

namespace madlib {
namespace modules {
namespace assoc_rules {

using madlib::dbconnector::postgres::madlib_get_typlenbyvalalign;
typedef struct perm_fctx
{
    bool*    flags;
    char*    positions;
    int32    pos_len;
    int32    num_elems;
    int32    num_calls;

    /* type information for the result type*/
    int16    typlen;
    bool     typbyval;
    char     typalign;
} perm_fctx;


/**
 * @brief   The init function for gen_rules_from_cfp.
 *
 * @param args      Two-element array.
 *                  args[0] is the text form of a closed frequent pattern.
 *                  args[1] is the number of items in the pattern.
 * @param max_call  The number of  will be generated.
 *
 * @return  The struct including the variables which will be used
 *          in the next call.
 *
 */
void *
gen_rules_from_cfp::SRF_init(AnyType &args) {
    perm_fctx     *myfctx        = NULL;
    char          *positions     = args[0].getAs<char*>();

    // allocate memory for user context
    myfctx            = new perm_fctx();
    myfctx->positions = positions;
    myfctx->pos_len   = static_cast<int32_t>(strlen(positions));
    myfctx->num_elems = args[1].getAs<int32>();
    myfctx->num_calls = (1 << myfctx->num_elems) - 2;
    myfctx->flags     = new bool[myfctx->num_elems];
    memset(myfctx->flags, 0, sizeof(bool) * myfctx->num_elems);
    // return type id is TEXTOID, get the related information
    madlib_get_typlenbyvalalign
        (TEXTOID, &myfctx->typlen, &myfctx->typbyval, &myfctx->typalign);

    return myfctx;
}


/**
 * @brief The next function for gen_rules_from_cfp.
 *
 * @param user_fctx    The pointer points to the struct including the
 *                     variables will be used in this function.
 * @param is_last_call Indicates if it's the last call.
 *
 * @return  A two-element array, corresponding to
 *             the left and right parts of an association rule.
 *
 */
AnyType
gen_rules_from_cfp::SRF_next(void *user_fctx, bool *is_last_call) {
    perm_fctx           *myfctx       = (perm_fctx*)user_fctx;
    int                 i             = 0;
    char                *pre_text;
    char                *post_text;
    char                *p_pre;
    char                *p_post;
    char                *p_begin;
    char                *p_cur;
    bool                is_cont;
    Datum               *result;
    char                **p_sel_pos;
    int                 len = 0;

    if (!is_last_call)
        throw std::invalid_argument("the paramter is_last_class should not be null");

    if (myfctx->num_calls <= 0) {
        *is_last_call = true;
        return Null();
    }

    // find the next permutation of the closed frequent pattern
    for (i = 0; i < myfctx->num_elems; ++i) {
        if (!myfctx->flags[i]) {
            myfctx->flags[i] = true;
            break;
        } else {
            myfctx->flags[i] = false;
        }
    }

    pre_text  = new char[myfctx->pos_len];
    post_text = new char[myfctx->pos_len];
    result    = new Datum[2];
    p_pre     = pre_text;
    p_post    = post_text;
    p_begin   = myfctx->positions;
    p_cur     = p_begin;
    is_cont   = myfctx->flags[0];
    memset(pre_text, 0, myfctx->pos_len * sizeof(char));
    memset(post_text, 0, myfctx->pos_len * sizeof(char));

    // get the left and right parts of the association rule, corresponding
    // to the current permutation
    for (i = 1; i < myfctx->num_elems; ++i) {
        /* move to the next comma */
        for (; *p_cur != ','; ++p_cur) ;

        /* the same with the last one, no need to copy */
        if (myfctx->flags[i] == is_cont) {
            ++p_cur;
            continue;
        } else {
            ++p_cur;
            len           = static_cast<int>(p_cur - p_begin);
            p_sel_pos     = is_cont ? &p_pre : &p_post;
            memcpy(*p_sel_pos, p_begin, len * sizeof(char));
            *p_sel_pos    += len;
            p_begin       = p_cur;
            is_cont       = !is_cont;
        }
    }

    if (is_cont) {
        p_sel_pos     = &p_pre;
        // remove the last comma
        *(p_post - 1) = '\0';
    } else {
        p_sel_pos     = &p_post;
        // remove the last comma
        *(p_pre - 1)  = '\0';
    }

    /* copy the remind text */
    memcpy(*p_sel_pos, p_begin,
        (myfctx->pos_len - (p_begin - myfctx->positions)) * sizeof(char));

    result[0] = PointerGetDatum(cstring_to_text(pre_text));
    result[1] = PointerGetDatum(cstring_to_text(post_text));

    ArrayHandle<text*> arr(construct_array(result, 2, TEXTOID,
            myfctx->typlen, myfctx->typbyval, myfctx->typalign));
    
    delete[] pre_text;
    delete[] post_text;

    --myfctx->num_calls;
    *is_last_call = false;
    return arr;
}

} // namespace assoc_rules

} // namespace modules

} // namespace madlib
