#ifndef MADLIB_POSTGRES_PGINTERFACE_HPP
#define MADLIB_POSTGRES_PGINTERFACE_HPP

#include <dbconnector/PGCommon.hpp>

extern "C" {
    #include "postgres.h"
    #include "fmgr.h"
} // extern "C"

namespace madlib {

namespace dbconnector {

class PGInterface : public AbstractDBInterface {
    friend class PGAllocator;

public:
    PGInterface(const FunctionCallInfo inFCinfo)
        : fcinfo(inFCinfo) { }
    
    AllocatorSPtr allocator(
        AbstractAllocator::Context inMemContext = AbstractAllocator::kFunction);
    
private:
    /**
     * The name is chosen so that PostgreSQL macros like PG_NARGS can be
     * used.
     */
    const FunctionCallInfo fcinfo;
};

} // namespace dbconnector

} // namespace madlib

#endif
