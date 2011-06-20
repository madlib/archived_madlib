#include <dbconnector/PGCompatibility.hpp>
#include <dbconnector/PGAllocator.hpp>
#include <dbconnector/PGArrayHandle.hpp>
#include <dbconnector/PGInterface.hpp>

extern "C" {
    #include <postgres.h>
    #include <fmgr.h>
    #include <miscadmin.h>
    #include <catalog/pg_type.h>
} // extern "C"

namespace madlib {

namespace dbconnector {

/**
 * @brief Allocate a double array
 */
MemHandleSPtr PGAllocator::allocateArray(uint32_t inNumElements,
    double * /* ignored */) const {
    
    return MemHandleSPtr(new PGArrayHandle(
        internalAllocateForArray(FLOAT8OID, inNumElements, sizeof(double))));
}

/**
 * @brief Deallocate an AbstractHandle
 */
void PGAllocator::deallocateHandle(MemHandleSPtr inHandle) const {
    shared_ptr<PGArrayHandle> arrayPtr = dynamic_pointer_cast<PGArrayHandle>(inHandle);
    
    if (arrayPtr)
        free(arrayPtr->mArray);
    else
        throw std::logic_error("Tried to deallocate invalid handle");
}

/**
 * @brief Construct an empty postgres array of a given size.
 * 
 * Set the length of the varlena header, set the element type, etc.
 */
ArrayType *PGAllocator::internalAllocateForArray(Oid inElementType,
    uint32_t inNumElements, size_t inElementSize) const {
    
    int64		size = inElementSize * inNumElements + ARR_OVERHEAD_NONULLS(1);
    ArrayType	*array;
    void		*arrayData;

    // Note: Except for the allocate call, this function does not call into the
    // PostgreSQL backend. We are only using macros here.
    
    array = static_cast<ArrayType *>(allocate(size));
    SET_VARSIZE(array, size);
    array->ndim = 1;
    array->dataoffset = 0;
    array->elemtype = inElementType;
    ARR_DIMS(array)[0] = inNumElements;
    ARR_LBOUND(array)[0] = 1;
    arrayData = ARR_DATA_PTR(array);
    std::memset(arrayData, 0, inElementSize * inNumElements);
    return array;
}

/**
 * @brief Allocate memory in our PostgreSQL memory context. Throws on fail.
 * 
 * In case allocation fails, throw an exception. At the boundary of the C++
 * layer, another PostgreSQL error will be raised (i.e., there will be at least
 * two errors on the PostgreSQL error handling stack).
 *
 * Important:
 * We call back into the database backend here, and these calls might cause
 * exceptions (e.g., an out-of-memory error). Exceptions in PostgreSQL/Greenplum
 * are implemented using longjmp. Since we do not want to leave the confines of
 * C++'s defined behavior, we insist on proper stack unwinding and thus
 * surround any access of the database backend with PG_TRY()/PG_CATCH() macros.
 *
 * By default, PostgreSQL's memory allocation happens in AllocSetAlloc from
 * utils/mmgr/aset.c.
 */
void *PGAllocator::allocate(const uint32_t inSize) const throw(std::bad_alloc) {
    
    void *ptr;
    bool errorOccurred = false;
    MemoryContext oldContext = NULL;
    MemoryContext aggContext = NULL;
 
    PG_TRY(); {
        if (mContext == kAggregate) {
            if (!AggCheckCallContext(mPGInterface->fcinfo, &aggContext))
                throw std::logic_error("Internal error: Tried to allocate "
                    "memory in aggregate context while not in aggregate");

            oldContext = MemoryContextSwitchTo(aggContext);
            ptr = palloc(inSize);
            MemoryContextSwitchTo(oldContext);
        } else {
            ptr = palloc(inSize);
        }
    } PG_CATCH(); {
        // Probably not necessary but we clean up after ourselves
        if (oldContext != NULL)
            MemoryContextSwitchTo(oldContext);
        
        errorOccurred = true;
    } PG_END_TRY();

    /*
     * PostgreSQL error messages can be stacked. So, it doesn't hurt to add
     * our own message. After unwinding the C++ stack, the PostgreSQL
     * exception will be re-thrown into the PostgreSQL C code.
     *
     * Throwing C++ exceptions inside a PG_CATCH block is not problematic
     * per se, but it does not hurt to keep the exception mechanisms clearly
     * separated.
     */
    if (errorOccurred)
        throw std::bad_alloc();
    
    return ptr;
}


/**
 * @brief Allocate memory in "our" PostgreSQL memory context. Never throw.
 *
 * In case allocation fails, do not throw an exception. Choosing a memory
 * context is not currently supported for
 * PGAllocator(const uint32_t, const std::nothrow_t&).
 *
 * We will hold back interrupts while in this function because we do not want
 * to flush the postgres error state, unless it is related to memory allocation.
 * (We have to flush the error state because we cannot throw exceptions within
 * allocate).
 *
 * Interrupts/Signals are only processed whenever the CHECK_FOR_INTERRUPTS()
 * macro is called (see miscadmin.h). Some PostgreSQL function implicitly call
 * this macro (a notable example being ereport -- the rationale here is that
 * the user should be able to abort queries that produce lots of output).
 * For the actual processing, see ProcessInterrupts() in tcop/postgres.c.
 * All aborting is done through the ereport mechanism.
 *
 * @note This function is also called by operator new (std::nothrow), which must
 *       not throw *any* exception.
 *
 * @see See also the notes for allocate(allocate(const uint32_t)
 */
void *PGAllocator::allocate(const uint32_t inSize, const std::nothrow_t&) const
    throw() {
    
    void *ptr = NULL;
    
    /*
     * HOLD_INTERRUPTS() and RESUME_INTERRUPTS() only change the value of a
     * global variable but have no other side effects. In particular, they do
     * not call CHECK_INTERRUPTS(). Hence, we are save to use these macros
     * outside of a PG_TRY() block.
     */
    
    HOLD_INTERRUPTS();
    PG_TRY(); {
        ptr = palloc(inSize);
    } PG_CATCH(); {
        /*
         * This cannot be due to an interrupt, so it's reasonably safe
         * to assume that the PG exception was a pure memory-allocation
         * issue. We ignore the error and flush the error state.
         * Flushing is necessary for leaving the error state (e.g., the memory
         * context is restored).
         */
        FlushErrorState();
        ptr = NULL;
    } PG_END_TRY();
    RESUME_INTERRUPTS();
    
    return ptr;
}

/**
 * @brief Free a block of memory previously allocated with allocate
 * 
 * FIXME: Think again whether our error-handling policy (ignoring) is appropriate
 * The default implementation calls AllocSetFree() from utils/mmgr/aset.c.
 * We must not throw errors, so we are essentially ignoring all errors.
 * This function is also called by operator delete (),
 * which must not throw *any* exceptions.
 *
 * @see See also the notes for PGAllocator::allocate(const uint32_t) and
 *      PGAllocator::allocate(const uint32_t, const std::nothrow_t&)
 */
void PGAllocator::free(void *inPtr) const throw() {
    /*
     * See allocate(const uint32_t, const std::nothrow_t&) why we disable
     * processing of interrupts.
     */
    HOLD_INTERRUPTS();
    PG_TRY(); {
        pfree(inPtr);
    } PG_CATCH(); {
        FlushErrorState();
    } PG_END_TRY();
    RESUME_INTERRUPTS();
}


} // namespace dbconnector

} // namespace madlib
