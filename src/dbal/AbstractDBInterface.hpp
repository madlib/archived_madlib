/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractDBInterface.hpp
 *
 *//* ----------------------------------------------------------------------- */

class AbstractDBInterface {
public:
    virtual ~AbstractDBInterface() { }
    
    // This function should be allowed to have side effects, so we do not
    // declare it as const.
    virtual AllocatorSPtr allocator(
        AbstractAllocator::Context inMemContext = AbstractAllocator::kFunction)
        = 0;
};
