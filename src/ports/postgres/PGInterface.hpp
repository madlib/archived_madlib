#ifndef MADLIB_PGINTERFACE_HPP
#define MADLIB_PGINTERFACE_HPP

#include <madlib/ports/postgres/postgres.hpp>

extern "C" {
    #include "postgres.h"
    #include "fmgr.h"
} // extern "C"

namespace madlib {

namespace ports {

namespace postgres {

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

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
