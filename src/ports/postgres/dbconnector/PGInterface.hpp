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
 *
 * There are two main issues when writing plug-in code for PostgreSQL in C++:
 * -# Exceptions in PostgreSQL are implemented using longjmp.\n
 *    .
 *    Since we must not leave the confines of C++'s defined behavior, we insist
 *    on proper stack unwinding and thus surround any access of the database
 *    backend with \c PG_TRY()/\c PG_CATCH() macros.\n
 *    .
 *    We never leave a \c PG_TRY()-block through:
 *    - A return call
 *    - A C++ exception
 *    .
 *    Moreover, in a \c PG_TRY()-block we do not
 *    - Declare or allocate variables
 *    - Call functions that might violate one of the above rules
 *    .
 * -# Memory leaks are only guaranteed not to occur if PostgreSQL memory
 *    allocation functions are used\n
 *    .
 *    PostgreSQL knows the concept of "memory contexts" such as current function
 *    call, current aggregate function, or current transaction. Memory
 *    allocation using \c palloc() always occurs within a specific memory
 *    context -- and once a memory context goes out of scope all memory
 *    associated with it will be deallocated (garbage collected).\n
 *    .
 *    As a second safety measure we therefore redefine <tt>operator new</tt> and
 *    <tt>operator delete</tt> to call \c palloc() and \c pfree(). (This is
 *    essentially an \b additional protection against leaking C++ code. Given
 *    1., no C++ destructor call will ever be missed.)
 *
 * @see PGAllocator
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
    
        /**
         * @brief Output a null-terminated C string.
         *
         * @param inMsg Null-terminated C string
         * @param inLength length of inMsg
         */
        void output(char *inMsg, uint32_t inLength);
        
    private:
        int mErrorLevel;
    };

    /**
     * @internal The name is chosen so that PostgreSQL macros like \c PG_NARGS
     *           can be used.
     */
    const FunctionCallInfo fcinfo;
};

} // namespace dbconnector

} // namespace madlib

#endif
