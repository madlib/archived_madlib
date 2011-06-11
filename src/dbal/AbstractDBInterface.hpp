/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractDBInterface.hpp
 *
 *//* ----------------------------------------------------------------------- */

class AbstractDBInterface {
public:
    AbstractDBInterface(
        std::streambuf *inInfoBuffer, std::streambuf *inErrorBuffer)
    :   out(inInfoBuffer),
        err(inErrorBuffer) { }
    
    virtual ~AbstractDBInterface() { }
    
    // This function should be allowed to have side effects, so we do not
    // declare it as const.
    virtual AllocatorSPtr allocator(
        AbstractAllocator::Context inMemContext = AbstractAllocator::kFunction)
        = 0;
    
    std::ostream out;
    std::ostream err;
};
