/* ----------------------------------------------------------------------- *//**
 *
 * @file UDF_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

/**
 * @brief User-defined function
 */
class UDF
  : public AbstractionLayer,
    public AbstractionLayer::Allocator {

public:
    UDF(FunctionCallInfo inFCInfo) : AbstractionLayer::Allocator(inFCInfo),
        dbout(&mOutStreamBuffer), dberr(&mErrStreamBuffer) { }

    template <class Function>
    static inline Datum call(FunctionCallInfo fcinfo);

private:
    OutputStreamBuffer<INFO> mOutStreamBuffer;
    OutputStreamBuffer<WARNING> mErrStreamBuffer;

protected:
    /**
     * @brief Informational output stream
     */
    std::ostream dbout;
    
    /**
     * @brief Warning and non-fatal error output stream
     */
    std::ostream dberr;
};

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
