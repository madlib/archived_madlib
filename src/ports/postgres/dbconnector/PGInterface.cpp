#include <dbconnector/PGInterface.hpp>
#include <dbconnector/PGAllocator.hpp>

namespace madlib {

namespace dbconnector {

inline AllocatorSPtr PGInterface::allocator(
    AbstractAllocator::Context inMemContext,
    AbstractAllocator::ZeroMemory inZeroMemory) {
    
    return AllocatorSPtr(new PGAllocator(this, inMemContext, inZeroMemory));
}

inline void PGInterface::PGOutputStreamBuffer::output(
    char *inMsg, uint32_t /* inLength */) {

    bool errorOccurred = false;
    
    PG_TRY(); {
        ereport(mErrorLevel,
            (errmsg("%s", inMsg))); // Don't supply strings as format strings!
    } PG_CATCH(); {
        errorOccurred = true;
    } PG_END_TRY();
    
    if (errorOccurred)
        throw std::runtime_error("An exception occured during message output.");
}

} // namespace dbconnector

} // namespace madlib
