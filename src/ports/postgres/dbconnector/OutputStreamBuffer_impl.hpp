/* ----------------------------------------------------------------------- *//**
 *
 * @file OutputStreamBuffer_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_OUTPUTSTREAMBUFFER_IMPL_HPP
#define MADLIB_POSTGRES_OUTPUTSTREAMBUFFER_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

/**
 * @brief Output a null-terminated C string.
 *
 * @param inMsg Null-terminated C string
 * @param inLength length of inMsg
 */
template <int ErrorLevel>
inline
void
OutputStreamBuffer<ErrorLevel>::output(char *inMsg,
    std::size_t /* inLength */) const {

    volatile bool errorOccurred = false;
    PG_TRY(); {
        ereport(ErrorLevel,
            (errmsg("%s", inMsg))); // Don't supply strings as format strings!
    } PG_CATCH(); {
        errorOccurred = true;
    } PG_END_TRY();

    if (errorOccurred)
        throw std::runtime_error("An exception occured during message output.");
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_OUTPUTSTREAMBUFFER_IMPL_HPP)
