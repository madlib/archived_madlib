#include <dbconnector/PGInterface.hpp>
#include <dbconnector/PGAllocator.hpp>

namespace madlib {

namespace dbconnector {

inline AllocatorSPtr PGInterface::allocator(
    AbstractAllocator::Context inMemContext) {
    
    return AllocatorSPtr(new PGAllocator(this, inMemContext));
}

} // namespace dbconnector

} // namespace madlib
