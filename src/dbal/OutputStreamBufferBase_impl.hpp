/* ----------------------------------------------------------------------- *//**
 *
 * @file OutputStreamBufferBase_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_OUTPUTSTREAMBUFFERBASE_IMPL_HPP
#define MADLIB_DBAL_OUTPUTSTREAMBUFFERBASE_IMPL_HPP

#include <cstring>      // Needed for std::memcpy

namespace madlib {

namespace dbal {

/**
 * @internal One extra byte is allocated for the terminating null character.
 */
template <class Derived, typename C, class Allocator>
OutputStreamBufferBase<Derived, C, Allocator>::OutputStreamBufferBase()
 :  mAllocator(), mStorageSize(kInitialBufferSize),
    mStorage(mAllocator.allocate(mStorageSize + 1)) {

    this->setp(mStorage, mStorage + mStorageSize);
}

template <class Derived, typename C, class Allocator>
OutputStreamBufferBase<Derived, C, Allocator>::~OutputStreamBufferBase() {
    mAllocator.deallocate(mStorage, mStorageSize + 1);
}

/**
 * @brief Output a string
 *
 * Subclasses are required to implement this method and to feed the message
 * the the the DBMS-specific logging routine.
 * @param inMsg Null-terminated string to be output.
 * @param inLength Length of inMsg (for convenience).
 */
template <class Derived, typename C, class Allocator>
void
OutputStreamBufferBase<Derived, C, Allocator>::output(C* inMsg,
    std::size_t inLength) const {

    static_cast<const Derived*>(this)->output(inMsg, inLength);
}

/**
 * @brief Handle case when stream receives a character that does not fit
 *        into the current buffer any more
 *
 * This function will allocate a new buffer of twice the old buffer size. If
 * the buffer has already the maximum size kMaxBufferSize, eof is returned
 * to indicate that the buffer cannot take any more input before a flush.
 */
template <class Derived, typename C, class Allocator>
typename OutputStreamBufferBase<Derived, C, Allocator>::int_type
OutputStreamBufferBase<Derived, C, Allocator>::overflow(int_type c) {
    if (this->pptr() >= this->epptr()) {
        if (mStorageSize >= kMaxBufferSize)
            return traits_type::eof();

        uint32_t newStorageSize = mStorageSize * 2;
        C* newStorage = mAllocator.allocate(newStorageSize + 1);
        std::copy(mStorage, mStorage + mStorageSize, newStorage);
        mAllocator.deallocate(mStorage, mStorageSize + 1);
        mStorage = newStorage;

        madlib_assert(
            this->pptr() == this->epptr() &&
            this->pptr() - this->pbase() == static_cast<int64_t>(mStorageSize),
            std::logic_error("Internal error: Logging buffer has become "
                "inconsistent"));

        this->setp(mStorage, mStorage + newStorageSize);
        this->pbump(mStorageSize);
        mStorageSize = newStorageSize;
    } else if (c == traits_type::eof())
       return traits_type::eof();

    *this->pptr() = static_cast<C>(c);
    this->pbump(1);
    return traits_type::not_eof(c);
}

/**
 * @brief Flush and reset buffer.
 */
template <class Derived, typename C, class Allocator>
int
OutputStreamBufferBase<Derived, C, Allocator>::sync() {
    std::ptrdiff_t length = this->pptr() - this->pbase();

    mStorage[length] = '\0';
    output(mStorage, length);

    this->setp(mStorage, mStorage + mStorageSize);
    return 0;
}

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_OUTPUTSTREAMBUFFERBASE_IMPL_HPP)
