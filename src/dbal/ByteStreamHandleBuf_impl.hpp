/* ----------------------------------------------------------------------- *//**
 *
 * @file ByteStreamHandleBuf_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_BYTESTREAMHANDLEBUF_IMPL_HPP
#define MADLIB_DBAL_BYTESTREAMHANDLEBUF_IMPL_HPP

namespace madlib {

namespace dbal {

template <class Storage, class CharType, bool IsMutable>
ByteStreamHandleBuf<Storage, CharType, IsMutable>::ByteStreamHandleBuf(
    size_t inSize)
  : mStorage(defaultAllocator().allocateByteString<
        dbal::FunctionContext, dbal::DoZero, dbal::ThrowBadAlloc>(inSize)),
    mPos(0) { }

template <class Storage, class CharType, bool IsMutable>
ByteStreamHandleBuf<Storage, CharType, IsMutable>::ByteStreamHandleBuf(
    const Storage_type& inStorage)
  : mStorage(inStorage), mPos(0) { }

template <class Storage, class CharType, bool IsMutable>
size_t
ByteStreamHandleBuf<Storage, CharType, IsMutable>::seek(
    size_t inPos) {

    mPos = inPos;

    if (inPos > size())
        return size_t(-1);
    else
        return mPos;
}

template <class Storage, class CharType, bool IsMutable>
const typename ByteStreamHandleBuf<Storage, CharType, IsMutable>::char_type*
ByteStreamHandleBuf<Storage, CharType, IsMutable>::ptr() const {
    return storage().ptr();
}

template <class Storage, class CharType, bool IsMutable>
size_t
ByteStreamHandleBuf<Storage, CharType, IsMutable>::size() const {
    return storage().size();
}

template <class Storage, class CharType, bool IsMutable>
size_t
ByteStreamHandleBuf<Storage, CharType, IsMutable>::tell() const {
    return mPos;
}

template <class Storage, class CharType, bool IsMutable>
void
ByteStreamHandleBuf<Storage, CharType, IsMutable>::setStorage(
    Storage_type& inStorage) {

    mStorage = inStorage;
}

template <class Storage, class CharType, bool IsMutable>
typename ByteStreamHandleBuf<Storage, CharType, IsMutable>::Storage_type&
ByteStreamHandleBuf<Storage, CharType, IsMutable>::storage() {
    return mStorage;
}

template <class Storage, class CharType, bool IsMutable>
const typename ByteStreamHandleBuf<Storage, CharType, IsMutable>::Storage_type&
ByteStreamHandleBuf<Storage, CharType, IsMutable>::storage() const {
    return mStorage;
}


// ByteStreamHandleBuf<Storage, CharType, Mutable>

template <class Storage, class CharType>
ByteStreamHandleBuf<Storage, CharType, Mutable>::ByteStreamHandleBuf(
    size_t inSize)
  : Base(inSize) { }

template <class Storage, class CharType>
ByteStreamHandleBuf<Storage, CharType, Mutable>::ByteStreamHandleBuf(
    const Storage_type& inStorage)
  : Base(inStorage) { }

template <class Storage, class CharType>
typename ByteStreamHandleBuf<Storage, CharType, Mutable>::char_type*
ByteStreamHandleBuf<Storage, CharType, Mutable>::ptr() {
    return const_cast<char_type*>(static_cast<Base*>(this)->ptr());
}

template <class Storage, class CharType>
void
ByteStreamHandleBuf<Storage, CharType, Mutable>::resize(
    size_t inSize, size_t inPivot) {

    if (inSize == this->size())
        return;

    char_type* oldPtr = this->ptr();
    size_t secondChunkStart
        = inPivot > this->size() ? this->size() : inPivot;
    size_t oldSize = this->size();
    ptrdiff_t delta = inSize - oldSize;

    // FIXME: Deallocate old memory!
    this->mStorage = defaultAllocator().allocateByteString<
        dbal::FunctionContext, dbal::DoZero, dbal::ThrowBadAlloc>(inSize);

    std::copy(oldPtr, oldPtr + secondChunkStart, this->ptr());
    std::copy(oldPtr + secondChunkStart, oldPtr + oldSize,
        this->ptr() + secondChunkStart + delta);
    std::fill(this->ptr() + secondChunkStart,
        this->ptr() + secondChunkStart + delta, 0);
}

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_BYTESTREAMHANDLEBUF_IMPL_HPP)
