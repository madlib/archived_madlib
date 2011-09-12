#include <dbconnector/PGArrayHandle.hpp>
#include <dbconnector/PGAllocator.hpp>

extern "C" {
    #include <postgres.h>
}


namespace madlib {

namespace dbconnector {

/**
 * @brief Constructor
 * 
 * @param inArray PostgreSQL ArrayType struct
 * @param inCtrl Copy array? If yes, the handle owns the memory and will
 *                  take care of cleaning up.
 */
PGArrayHandle::PGArrayHandle(ArrayType *inArray, MemoryController inCtrl)
    : AbstractHandle(inCtrl), mArray(inArray) {
    
    if (inCtrl == kSelf) {
        // Use default allocator (this will allocate memory in the default
        // postgres context, i.e., the function-call context)

        mArray = static_cast<ArrayType *>(
            PGAllocator::defaultAllocator().allocate(VARSIZE(inArray)) );
        std::memcpy(mArray, inArray, VARSIZE(inArray));
    }
}

PGArrayHandle::~PGArrayHandle() {
    if (memoryController() == kSelf)
        PGAllocator::defaultAllocator().free(mArray);
}

} // namespace dbconnector

} // namespace madlib
