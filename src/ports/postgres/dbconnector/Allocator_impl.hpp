/* ----------------------------------------------------------------------- *//**
 *
 * @file Allocator_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_ALLOCATOR_IMPL_HPP
#define MADLIB_POSTGRES_ALLOCATOR_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

/**
 * @brief Construct an empty postgres array of the given size.
 *
 * This calls allocate() to allocate a block of memory and then initializes
 * PostgreSQL meta information.
 *
 * @note
 *     There is a template-overloaded version with defaults
 *     <tt>MC = dbal::FunctionContext</tt>, <tt>ZM = dbal::DoZero</tt>,
 *     <tt>F = dbal::ThrowBadAlloc</tt>
 */
template <typename T, std::size_t Dimensions, dbal::MemoryContext MC,
    dbal::ZeroMemory ZM, dbal::OnMemoryAllocationFailure F>
inline
MutableArrayHandle<T>
Allocator::internalAllocateArray(
    const std::array<std::size_t, Dimensions>& inNumElements) const {

    std::size_t numElements = Dimensions ? 1 : 0;
    for (std::size_t i = 0; i < Dimensions; ++i)
        numElements *= inNumElements[i];

    /*
     * Check that the size will not exceed addressable memory. Therefore, the
     * following precondition has to hold:
     * ((std::numeric_limits<std::size_t>::max()
     *     - ARR_OVERHEAD_NONULLS(Dimensions)) / inElementSize >= numElements)
     */
    if ((std::numeric_limits<std::size_t>::max()
        - ARR_OVERHEAD_NONULLS(Dimensions)) / sizeof(T) < numElements)
        throw std::bad_alloc();

    std::size_t size = sizeof(T) * numElements
        + ARR_OVERHEAD_NONULLS(Dimensions);
    ArrayType *array;

    // Note: Except for the allocate call, the following statements do not call
    // into the PostgreSQL backend. We are only using macros here.

    // PostgreSQL requires that all memory is overwritten with zeros. So
    // we ingore ZM here
    array = static_cast<ArrayType*>(allocate<MC, dbal::DoZero, F>(size));

    SET_VARSIZE(array, size);
    array->ndim = Dimensions;
    array->dataoffset = 0;
    array->elemtype = TypeTraits<T>::oid;
    for (std::size_t i = 0; i < Dimensions; ++i) {
        ARR_DIMS(array)[i] = static_cast<int>(inNumElements[i]);
        ARR_LBOUND(array)[i] = 1;
    }

    return MutableArrayHandle<T>(array);
}

#define MADLIB_ALLOCATE_ARRAY_DEF(z, n, _ignored) \
    template <typename T> \
    inline \
    MutableArrayHandle<T> \
    Allocator::allocateArray( \
        BOOST_PP_ENUM_PARAMS_Z(z, BOOST_PP_INC(n), std::size_t inDim) \
    ) const { \
        std::array<std::size_t, BOOST_PP_INC(n)> numElements = {{ \
            BOOST_PP_ENUM_PARAMS_Z(z, BOOST_PP_INC(n), inDim) \
        }}; \
        return internalAllocateArray<T, BOOST_PP_INC(n), \
            dbal::FunctionContext, dbal::DoZero, dbal::ThrowBadAlloc> \
            (numElements); \
    } \
    \
    template <typename T, dbal::MemoryContext MC, \
        dbal::ZeroMemory ZM, dbal::OnMemoryAllocationFailure F> \
    inline \
    MutableArrayHandle<T> \
    Allocator::allocateArray( \
        BOOST_PP_ENUM_PARAMS_Z(z, BOOST_PP_INC(n), std::size_t inDim) \
    ) const { \
        std::array<std::size_t, BOOST_PP_INC(n)> numElements = {{ \
            BOOST_PP_ENUM_PARAMS_Z(z, BOOST_PP_INC(n), inDim) \
        }}; \
        return internalAllocateArray<T, BOOST_PP_INC(n), MC, ZM, F> \
        (numElements); \
    }
BOOST_PP_REPEAT(MADLIB_MAX_ARRAY_DIMS, MADLIB_ALLOCATE_ARRAY_DEF,
    0 /* ignored */)
#undef MADLIB_ALLOCATE_ARRAY_DEF

/**
 * @brief Allocate a block of memory
 *
 * @return The address of a 16-byte aligned block of memory large enough to hold
 *     \c inSize bytes. On all supported platforms, 16-byte alignment is enough
 *     for any arbitrary operation.
 */
template <dbal::MemoryContext MC, dbal::ZeroMemory ZM,
    dbal::OnMemoryAllocationFailure F>
inline
void *
Allocator::allocate(size_t inSize) const {
    return internalAllocate<MC, ZM, F, NewAllocation>(NULL, inSize);
}

/**
 * @brief Change the size of a block of memory previously allocated with
 *     Allocator allocation functions
 *
 * There is no guerantee that the returned pointer is the same as \c inPtr.
 *
 * @param inPtr Pointer to a memory block previously allocated with allocate or
 *     reallocate.
 * @param inSize Requested size for the reallocated memory block
 *
 * @return The address of a 16-byte aligned block of memory large enough to hold
 *     \c inSize bytes. On all supported platforms, 16-byte alignment is enough
 *     for any arbitrary operation.
 */
template <dbal::MemoryContext MC, dbal::ZeroMemory ZM,
    dbal::OnMemoryAllocationFailure F>
inline
void *
Allocator::reallocate(void *inPtr, const size_t inSize) const {
    return internalAllocate<MC, ZM, F, Reallocation>(inPtr, inSize);
}


/**
 * @brief Free a block of memory previously allocated with
 *     Allocator allocation functions
 *
 * @internal
 *     This function uses the PostgreSQL pfree() macro. This calls
 *     MemoryContextFreeImpl, which again calls, by default, AllocSetFree() from
 *     utils/mmgr/aset.c.
 *
 * We must not throw errors, so we are essentially ignoring all errors.
 * This function is also called by operator delete(),
 * which must not throw *any* exceptions.
 *
 * @param inPtr Pointer to a memory block previously allocated with allocate,
 *     reallocate or allocateArray. If a null pointer is passed as argument, no
 *     action occurs. (std::free has the same behavior.)
 *
 * @see See also the notes for PGAllocator::allocate(const size_t) and
 *      PGAllocator::allocate(const size_t, const std::nothrow_t&)
 */
template <dbal::MemoryContext MC>
inline
void
Allocator::free(void *inPtr) const {
    if (inPtr == NULL)
        return;

    /*
     * See allocate(const size_t, const std::nothrow_t&) why we disable
     * processing of interrupts.
     */
    HOLD_INTERRUPTS();
    PG_TRY(); {
        pfree(unaligned(inPtr));
    } PG_CATCH(); {
        FlushErrorState();
    } PG_END_TRY();
    RESUME_INTERRUPTS();
}

/**
 * @brief Thin wrapper around \c palloc() that returns a 16-byte-aligned
 *     pointer.
 *
 * @internal
 *     This function uses the PostgreSQL palloc() and palloc0() macros. They
 *     call MemoryContextAllocImpl() or MemoryContextAllocZeroImpl(),
 *     respectively, which then call, by default, AllocSetAlloc() from
 *     utils/mmgr/aset.c.
 *
 * Unless <tt>MAXIMUM_ALIGNOF >= 16</tt>, we waste 16 additional bytes of
 * memory. The call to \c palloc() might throw a PostgreSQL exception. Thus,
 * this function should only be used inside a \c PG_TRY() block.
 */
template <dbal::ZeroMemory ZM>
inline
void *
Allocator::internalPalloc(size_t inSize) const {
#if MAXIMUM_ALIGNOF >= 16
    return (ZM == dbal::DoZero) ? palloc0(inSize) : palloc(inSize);
#else
    if (inSize > std::numeric_limits<size_t>::max() - 16)
        return NULL;

    /* Precondition: inSize <= std::numeric_limits<size_t>::max() - 16 */
    const size_t size = inSize + 16;
    void *raw = (ZM == dbal::DoZero) ? palloc0(size) : palloc(size);
    return makeAligned(raw);
#endif
}

/**
 * @brief Thin wrapper around \c repalloc() that returns a 16-byte-aligned
 *     pointer.
 *
 * @tparam ZM Initialize memory block by overwriting with zeros?
 *
 * @internal
 *     This function uses the PostgreSQL repalloc() macro. This calls
 *     MemoryContextReallocImpl, which again calls, by default,
 *     AllocSetRealloc() from utils/mmgr/aset.c.
 *
 * Unless <tt>MAXIMUM_ALIGNOF >= 16</tt>, we waste 16 additional bytes of
 * memory. The call to \c repalloc() might throw a PostgreSQL exception. Thus,
 * this function should only be used inside a \c PG_TRY() block.
 */
template <dbal::ZeroMemory ZM>
inline
void *
Allocator::internalRePalloc(void *inPtr, size_t inSize) const {
#if MAXIMUM_ALIGNOF >= 16
    return repalloc(inPtr, inSize);
#else
    if (inSize > std::numeric_limits<size_t>::max() - 16) {
        pfree(unaligned(inPtr));
        return NULL;
    }

    /* Precondition: inSize <= std::numeric_limits<size_t>::max() - 16 */
    const size_t size = inSize + 16;
    void *raw = repalloc(unaligned(inPtr), size);

    if (ZM == dbal::DoZero) {
        std::fill(
            static_cast<char*>(raw),
            static_cast<char*>(raw) + inSize, 0);
    }

    return makeAligned(raw);
#endif
}

/**
 * @internal
 * @brief Return next 16-byte boundary after inPtr and store inPtr in word
 *     immediately before that
 */
inline
void *
Allocator::makeAligned(void *inPtr) const {
    if (inPtr == NULL) return NULL;

    /*
     * Precondition: reinterprete_cast<size_t>(raw) % sizeof(void**) == 0,
     * i.e., the memory returned by palloc is at least aligned on word size
     * boundaries. That ensures that the word immediately preceding raw belongs
     * to us an can be written to safely.
     */
    void *aligned = reinterpret_cast<void*>(
        (reinterpret_cast<uintptr_t>(inPtr) & ~(uintptr_t(15))) + 16);
    *(reinterpret_cast<void**>(aligned) - 1) = inPtr;
    return aligned;
}

/**
 * @internal
 * @brief Return the address of memory block that corresponds to the given
 *     16-byte aligned address
 *
 * Unless <tt>MAXIMUM_ALIGNOF >= 16</tt>, we free the block of memory pointed to
 * by the word immediately in front of the memory pointed to by \c inPtr.
 */
inline
void *
Allocator::unaligned(void *inPtr) const {
#if MAXIMUM_ALIGNOF >= 16
    return inPtr;
#else
    return (*(reinterpret_cast<void**>(inPtr) - 1));
#endif
}

/**
 * @brief Allocate memory in our PostgreSQL memory context. Throws on fail.
 *
 * @tparam MC Which memory context to allocate in?
 * @tparam ZM Initialize memory block by overwriting with zeros?
 * @tparam F What to do in case of failure?
 * @tparam R Do a reallication or a new allocation?
 *
 * If <tt>F == ThrowBadAlloc</tt>: In case allocation fails, throw an exception.
 * At the boundary of the C++ layer, another PostgreSQL error will be raised
 * (i.e., there will be at least two errors on the PostgreSQL error handling
 * stack).
 *
 * If <tt>F == ReturnNULL</tt>: In case allocation fails, do not throw an
 * exception. ALso, make sure we do not leave in an error state. Instead, we
 * just return NULL.
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
 * By default, PostgreSQL's memory allocation happens in AllocSetAlloc from
 * utils/mmgr/aset.c.
 *
 * @see PGInterface for information on necessary precautions when writing
 *      PostgreSQL plug-in code in C++.
 *
 * @exception std::bad_alloc if the allocation fails and
 *            <tt>F == ThrowBadAlloc</tt>
 */
template <
    dbal::MemoryContext MC,
    dbal::ZeroMemory ZM,
    dbal::OnMemoryAllocationFailure F,
    Allocator::ReallocateMemory R>
inline
void *
Allocator::internalAllocate(void *inPtr, const size_t inSize) const {
    // Avoid warning that inPtr is not used if R == NewAllocation
    (void) inPtr;

    void *ptr;
    bool errorOccurred = false;
    MemoryContext oldContext = NULL;
    MemoryContext aggContext = NULL;

    if (F == dbal::ReturnNULL) {
        /*
         * HOLD_INTERRUPTS() and RESUME_INTERRUPTS() only change the value of a
         * global variable but have no other side effects. In particular, they
         * do not call CHECK_INTERRUPTS(). Hence, we are save to use these
         * macros outside of a PG_TRY() block.
         */
        HOLD_INTERRUPTS();
    }

    PG_TRY(); {
        if (MC == dbal::AggregateContext) {
            if (!AggCheckCallContext(fcinfo, &aggContext))
                errorOccurred = true;
            else {
                oldContext = MemoryContextSwitchTo(aggContext);
                ptr = (R == Reallocation) ? internalRePalloc<ZM>(inPtr, inSize)
                                          : internalPalloc<ZM>(inSize);
                MemoryContextSwitchTo(oldContext);
            }
        } else {
            ptr = R ? internalRePalloc<ZM>(inPtr, inSize)
                    : internalPalloc<ZM>(inSize);
        }
    } PG_CATCH(); {
        if (F == dbal::ReturnNULL) {
            /*
             * This cannot be due to an interrupt, so it's reasonably safe
             * to assume that the PG exception was a pure memory-allocation
             * issue. We ignore the error and flush the error state.
             * Flushing is necessary for leaving the error state (e.g., the memory
             * context is restored).
             */
            FlushErrorState();
            ptr = NULL;
        } else {
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
        }
    } PG_END_TRY();

    if (errorOccurred) {
        PG_TRY(); {
            // Clean up after ourselves
            if (oldContext != NULL)
                MemoryContextSwitchTo(oldContext);
        } PG_CATCH(); {
            if (F == dbal::ReturnNULL) {
                // We tried to clean up after ourselves. If this fails, we can
                // only ignore the issue.
                FlushErrorState();
            }
            // Else do nothing. We will add a bad-allocation exception on top of
            // the existing PostgreSQL exception stack.
        } PG_END_TRY();
    }

    if (F == dbal::ReturnNULL) {
        RESUME_INTERRUPTS();
    }

    if (errorOccurred || !ptr)
        // We do not want to interleave PG exceptions and C++ exceptions.
        throw std::bad_alloc();

    return ptr;
}

/**
 * @brief Get the default allocator
 */
inline
Allocator&
defaultAllocator() {
    static Allocator sDefaultAllocator(NULL);
    return sDefaultAllocator;
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_ALLOCATOR_IMPL_HPP)
