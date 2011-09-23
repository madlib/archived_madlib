#include <dbconnector/PGCompatibility.hpp>
#include <dbconnector/PGAllocator.hpp>
#include <dbconnector/PGArrayHandle.hpp>
#include <dbconnector/PGInterface.hpp>

extern "C" {
    #include <postgres.h>
    #include <fmgr.h>
    #include <miscadmin.h>
    #include <catalog/pg_type.h>
    #include <utils/memutils.h>
} // extern "C"

namespace madlib {

namespace dbconnector {

/**
 * @brief Allocate a double array
 */
MemHandleSPtr PGAllocator::allocateArray(uint64_t inNumElements,
    double * /* ignored */) const {
    
    return MemHandleSPtr(new PGArrayHandle(
        internalAllocateForArray(FLOAT8OID, inNumElements, sizeof(double))));
}

/**
 * @internal
 * @brief Construct an empty postgres array of a given size.
 * 
 * Set the length of the varlena header, set the element type, etc.
 */
ArrayType *PGAllocator::internalAllocateForArray(Oid inElementType,
    uint64_t inNumElements, size_t inElementSize) const {
    
    /*
     * Check that the size will not exceed addressable memory. Therefore, the
     * following precondition has to hold:
     * ((std::numeric_limits<size_t>::max() - ARR_OVERHEAD_NONULLS(1)) /
     *     inElementSize >= inNumElements)
     */
    if ((std::numeric_limits<size_t>::max() - ARR_OVERHEAD_NONULLS(1)) /
            inElementSize < inNumElements)
        throw std::bad_alloc();
    
    size_t		size = inElementSize * inNumElements + ARR_OVERHEAD_NONULLS(1);
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
 * @internal
 * @brief Thin wrapper around \c palloc() that returns a 16-byte-aligned
 *     pointer.
 * 
 * Unless <tt>MAXIMUM_ALIGNOF >= 16</tt>, we waste 16 additional bytes of
 * memory. The call to \c palloc() might throw a PostgreSQL exception. Thus,
 * this function should only be used inside a \c PG_TRY() block.
 *
 * We store the address returned by the call to \c palloc() in the word
 * immediately before the aligned pointer that we return.
 */
void *PGAllocator::internalPalloc(size_t inSize, bool inZero) {
#if MAXIMUM_ALIGNOF >= 16
    return inZero ? palloc0(inSize) : palloc(inSize);
#else
    if (inSize > std::numeric_limits<size_t>::max() - 16)
        return NULL;
    
    /* Precondition: inSize <= std::numeric_limits<size_t>::max() - 16 */
    const size_t size = inSize + 16;
    void *raw = inZero ? palloc0(size) : palloc(size);
    if (raw == 0) return 0;
    
    /*
     * Precondition: reinterprete_cast<size_t>(raw) % sizeof(void**) == 0,
     * i.e., the memory returned by palloc is at least aligned on word size
     * boundaries. That ensures that the word immediately preceding raw belongs
     * to us an can be written to safely.
     */
    void *aligned = reinterpret_cast<void*>(
        (reinterpret_cast<uintptr_t>(raw) & ~(uintptr_t(15))) + 16);
    *(reinterpret_cast<void**>(aligned) - 1) = raw;
    return aligned;
#endif
}

/**
 * @internal
 * @brief Thin wrapper around \c pfree() that frees 16-byte allocated memory.
 *
 * Unless <tt>MAXIMUM_ALIGNOF >= 16</tt>, we free the block of memory pointed to
 * by the word immediately in front of the memory pointed to by \c inPtr.
 */
void PGAllocator::internalPfree(void *inPtr) {
#if MAXIMUM_ALIGNOF >= 16
    pfree(inPtr);
#else
    pfree(*(reinterpret_cast<void**>(inPtr) - 1));    
#endif
}

/**
 * @brief Allocate memory in our PostgreSQL memory context. Throws on fail.
 * 
 * In case allocation fails, throw an exception. At the boundary of the C++
 * layer, another PostgreSQL error will be raised (i.e., there will be at least
 * two errors on the PostgreSQL error handling stack).
 *
 * By default, PostgreSQL's memory allocation happens in AllocSetAlloc from
 * utils/mmgr/aset.c.
 *
 * @see PGInterface for information on necessary precautions when writing
 *      PostgreSQL plug-in code in C++.
 */
void *PGAllocator::allocate(const size_t inSize) const throw(std::bad_alloc) {
    void *ptr;
    bool errorOccurred = false;
    MemoryContext oldContext = NULL;
    MemoryContext aggContext = NULL;

    PG_TRY(); {
        if (mContext == kAggregate) {
            if (!AggCheckCallContext(mPGInterface->fcinfo, &aggContext))
                errorOccurred = true;
            else {
                oldContext = MemoryContextSwitchTo(aggContext);
                ptr = internalPalloc(inSize, mZeroMemory);
                MemoryContextSwitchTo(oldContext);
            }
        } else {
            ptr = internalPalloc(inSize, mZeroMemory);
        }
    } PG_CATCH(); {
        /*
         * PostgreSQL error messages can be stacked. So, it doesn't hurt to add
         * our own message. After unwinding the C++ stack, the PostgreSQL
         * exception will be re-thrown into the PostgreSQL C code.
         *
         * Throwing C++ exceptions inside a PG_CATCH block is not problematic
         * per se, but it is good practise to keep the exception mechanisms clearly
         * separated.
         */

        errorOccurred = true;
    } PG_END_TRY();

    if (errorOccurred) {
        PG_TRY(); {
            // Clean up after ourselves
            if (oldContext != NULL)
                MemoryContextSwitchTo(oldContext);
        } PG_CATCH(); {
            // Do nothing. We will add a bad-allocation exception on top of
            // the existing PostgreSQL exception stack.
        }; PG_END_TRY();
    }
    
    if (errorOccurred || !ptr)
        // We do not want to interleave PG exceptions and C++ exceptions.
        throw std::bad_alloc();
    
    return ptr;
}


/**
 * @brief Allocate memory in "our" PostgreSQL memory context. Never throw.
 *
 * In case allocation fails, do not throw an exception. ALso, make sure we do
 * not leave in an error state. Instead, we just return NULL.
 *
 * We will hold back interrupts while in this function because we do not want
 * to flush the postgres error state unless it is related to memory allocation.
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
 * @see See also the notes for allocate(allocate(const size_t)
 */
void *PGAllocator::allocate(const size_t inSize, const std::nothrow_t&) const
    throw() {
    
    void *ptr = NULL;
    bool errorOccurred = false;
    MemoryContext oldContext = NULL;
    MemoryContext aggContext = NULL;
    
    /*
     * HOLD_INTERRUPTS() and RESUME_INTERRUPTS() only change the value of a
     * global variable but have no other side effects. In particular, they do
     * not call CHECK_INTERRUPTS(). Hence, we are save to use these macros
     * outside of a PG_TRY() block.
     */
    
    HOLD_INTERRUPTS();
    PG_TRY(); {
        if (mContext == kAggregate) {
            if (!AggCheckCallContext(mPGInterface->fcinfo, &aggContext))
                errorOccurred = true;
            else {
                oldContext = MemoryContextSwitchTo(aggContext);
                ptr = internalPalloc(inSize, mZeroMemory);
                MemoryContextSwitchTo(oldContext);
            }
        } else {
            ptr = internalPalloc(inSize, mZeroMemory);
        }
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
    
    if (errorOccurred) {
        PG_TRY(); {
            // Clean up after ourselves
            if (oldContext != NULL)
                MemoryContextSwitchTo(oldContext);
        } PG_CATCH(); {
            // We tried to clean up after ourselves. If this fails, we can only
            // ignore the issue.
            FlushErrorState();
        }; PG_END_TRY();
    }
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
 * @see See also the notes for PGAllocator::allocate(const size_t) and
 *      PGAllocator::allocate(const size_t, const std::nothrow_t&)
 */
void PGAllocator::free(void *inPtr) const throw() {
    /*
     * See allocate(const size_t, const std::nothrow_t&) why we disable
     * processing of interrupts.
     */
    HOLD_INTERRUPTS();
    PG_TRY(); {
        internalPfree(inPtr);
    } PG_CATCH(); {
        FlushErrorState();
    } PG_END_TRY();
    RESUME_INTERRUPTS();
}

} // namespace dbconnector

namespace dbal {

/**
 * @brief Return the default allocator used by operator new and operator delete.
 */
AbstractAllocator &defaultAllocator() {
    return dbconnector::PGAllocator::defaultAllocator();
}

} // namespace dbal

} // namespace madlib
