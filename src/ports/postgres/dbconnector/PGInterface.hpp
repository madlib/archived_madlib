#ifndef MADLIB_POSTGRES_PGINTERFACE_HPP
#define MADLIB_POSTGRES_PGINTERFACE_HPP

#include <dbconnector/PGCommon.hpp>

extern "C" {
    #include "postgres.h"
    #include "fmgr.h"
} // extern "C"

namespace madlib {

namespace dbconnector {

/**
 * @brief PostgreSQL database interface
 */
class PGInterface : public AbstractDBInterface {
    friend class PGAllocator;

public:
    PGInterface(const FunctionCallInfo inFCinfo)
    :   AbstractDBInterface(
            new PGOutputStreamBuffer(INFO),
            new PGOutputStreamBuffer(WARNING)),
        fcinfo(inFCinfo) {
        
        // Observe: This only works because PostgreSQL does not use multiple
        // threads for UDFs
        arma::set_log_stream(mArmadilloOut);
    }

    ~PGInterface() {
        // We allocated the streambuf objects in the initialization list of
        // the constructor. Hence, we also have to clean them up.
        delete out.rdbuf();
        delete err.rdbuf();
    }
    
    AllocatorSPtr allocator(
        AbstractAllocator::Context inMemContext = AbstractAllocator::kFunction);
    
private:
    /**
     * @brief Stream buffer that dispatches all output to PostgreSQL's ereport
     *        function
     */
    class PGOutputStreamBuffer : public AbstractOutputStreamBuffer<char> {
    public:
        PGOutputStreamBuffer(int inErrorLevel) : mErrorLevel(inErrorLevel) { }
    
        void output(char *inMsg, uint32_t inLength);
        
    private:
        int mErrorLevel;
    };

    /**
     * @internal The name is chosen so that PostgreSQL macros like PG_NARGS can
     *           be used.
     */
    const FunctionCallInfo fcinfo;
};

} // namespace dbconnector

} // namespace madlib

#endif
