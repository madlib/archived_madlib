/* ----------------------------------------------------------------------- *//**
 *
 * @file OutputStreamBuffer_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

/**
 * @brief Output a null-terminated C string.
 *
 * @param inMsg Null-terminated C string
 * @param inLength length of inMsg
 */
template <int ErrorLevel>
inline
void
AbstractionLayer::OutputStreamBuffer<ErrorLevel>::output(char *inMsg,
    uint32_t /* inLength */) const {
    
    bool errorOccurred = false;
    
    PG_TRY(); {
        ereport(ErrorLevel,
            (errmsg("%s", inMsg))); // Don't supply strings as format strings!
    } PG_CATCH(); {
        errorOccurred = true;
    } PG_END_TRY();
    
    if (errorOccurred)
        throw std::runtime_error("An exception occured during message output.");
}

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
