/* ----------------------------------------------------------------------- *//**
 *
 * @file ByteStream_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_BYTESTREAM_IMPL_HPP
#define MADLIB_DBAL_BYTESTREAM_IMPL_HPP

namespace madlib {

namespace dbal {

// ByteStream<StreamBuf, TypeTraits, IsMutable>::DryRun

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
ByteStream<StreamBuf, TypeTraits, IsMutable>::DryRun::DryRun(
    ByteStream& inStream)
  : mStream(inStream), mIsIn(true) {

    mStream.enterDryRun();
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
ByteStream<StreamBuf, TypeTraits, IsMutable>::DryRun::~DryRun() {
    leave();
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
void
ByteStream<StreamBuf, TypeTraits, IsMutable>::DryRun::leave() {
    if (mIsIn) {
        mStream.leaveDryRun();
        mIsIn = false;
    }
}


// ByteStream<StreamBuf, bool IsMutable>

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
ByteStream<StreamBuf, TypeTraits, IsMutable>::ByteStream(
    StreamBuf_type* inStreamBuf)
  : mStreamBuf(inStreamBuf), mDryRun(false) { }

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
template <class T>
inline
const T*
ByteStream<StreamBuf, TypeTraits, IsMutable>::read(size_t inCount) {
    this->seek<TypeTraits<typename boost::remove_cv<T>::type>::alignment>(
        0, std::ios_base::cur);
    const T* pointer = this->available() >= inCount * sizeof(T)
        ? reinterpret_cast<const T*>(this->ptr() + this->tell())
        : NULL;
    this->seek(inCount * sizeof(T), std::ios_base::cur);
    return pointer;
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
template <std::size_t Alignment>
inline
size_t
ByteStream<StreamBuf, TypeTraits, IsMutable>::seek(std::ptrdiff_t inOffset,
    std::ios_base::seekdir inDir) {

    BOOST_STATIC_ASSERT_MSG(
        (Alignment & (Alignment - 1)) == 0 && Alignment > 0,
        "Alignment must be a power of 2.");
    madlib_assert(
        reinterpret_cast<uint64_t>(this->ptr()) % Alignment == 0,
        std::logic_error("ByteString improperly aligned for "
            "alignment request in seek()."));

    size_t newPos =
        inDir == std::ios_base::beg
        ? 0
        : (inDir == std::ios_base::cur
            ? this->tell()
            : this->size());

    if (inOffset >= 0)
        newPos += static_cast<size_t>(inOffset);
    else if (static_cast<size_t>(-inOffset) > newPos)
        newPos = 0;
    else
        newPos -= static_cast<size_t>(-inOffset);
    newPos = ((newPos - 1) & ~(Alignment - 1)) + Alignment;

    return this->seek(newPos);
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
size_t
ByteStream<StreamBuf, TypeTraits, IsMutable>::seek(std::ptrdiff_t inOffset,
    std::ios_base::seekdir inDir) {

    return seek<1>(inOffset, inDir);
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
size_t
ByteStream<StreamBuf, TypeTraits, IsMutable>::seek(size_t inPos) {
    return mStreamBuf->seek(inPos);
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
size_t
ByteStream<StreamBuf, TypeTraits, IsMutable>::size() const {
    return mStreamBuf->size();
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
std::ios_base::iostate
ByteStream<StreamBuf, TypeTraits, IsMutable>::rdstate() const {
    return (this->tell() <= this->size()
        ? std::ios_base::goodbit
        : std::ios_base::eofbit);
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
bool
ByteStream<StreamBuf, TypeTraits, IsMutable>::eof() const {
    return rdstate() & std::ios_base::eofbit;
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
size_t
ByteStream<StreamBuf, TypeTraits, IsMutable>::available() const {
    if (this->size() < this->tell())
        return 0;
    else
        return this->size() - this->tell();
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
size_t
ByteStream<StreamBuf, TypeTraits, IsMutable>::tell() const {
    return mStreamBuf->tell();
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
const typename ByteStream<StreamBuf, TypeTraits, IsMutable>::char_type*
ByteStream<StreamBuf, TypeTraits, IsMutable>::ptr() const {
    return mStreamBuf->ptr();
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
bool
ByteStream<StreamBuf, TypeTraits, IsMutable>::isInDryRun() const {
    return mDryRun > 0;
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
void
ByteStream<StreamBuf, TypeTraits, IsMutable>::enterDryRun() {
    ++mDryRun;
}

template <class StreamBuf, template <class T> class TypeTraits, bool IsMutable>
inline
void
ByteStream<StreamBuf, TypeTraits, IsMutable>::leaveDryRun() {
    madlib_assert(mDryRun > 0, std::logic_error("Non-positive dry-mode "
        "counter detected."));
    --mDryRun;
}

// ByteStream<StreamBuf, Mutable>

template <class StreamBuf, template <class T> class TypeTraits>
ByteStream<StreamBuf, TypeTraits, Mutable>::ByteStream(
    StreamBuf_type* inStreamBuf)
  : Base(inStreamBuf) { }

template <class StreamBuf, template <class T> class TypeTraits>
template <class T>
inline
T*
ByteStream<StreamBuf, TypeTraits, Mutable>::read(size_t inCount) {
    return const_cast<T*>(static_cast<Base*>(this)->template read<T>(inCount));
}

// operator>>(ByteStream<StreamBuf>&, ...)

template <
    class StreamBuf,
    template <class T> class TypeTraits,
    bool StreamBufIsMutable,
    bool ReferenceIsMutable,
    typename T>
ByteStream<StreamBuf, TypeTraits, StreamBufIsMutable>&
operator>>(
    ByteStream<StreamBuf, TypeTraits, StreamBufIsMutable>& inStream,
    Ref<T, ReferenceIsMutable>& inReference) {

    typedef typename Ref<T, ReferenceIsMutable>::val_type val_type;

    val_type* data = inStream.template read<val_type>();
    if (!inStream.isInDryRun())
        inReference.rebind(data);
    return inStream;
}

template <
    class StreamBuf,
    template <class T> class TypeTraits,
    bool StreamBufIsMutable,
    bool TransparentHandleIsMutable,
    typename EigenType>
ByteStream<StreamBuf, TypeTraits, StreamBufIsMutable>&
operator>>(
    ByteStream<StreamBuf, TypeTraits, StreamBufIsMutable>& inStream,
    dbal::eigen_integration::HandleMap<
        EigenType,
        TransparentHandle<typename EigenType::Scalar, TransparentHandleIsMutable>
    >& inReference) {

    typedef typename TransparentHandle<typename EigenType::Scalar, TransparentHandleIsMutable>::val_type val_type;

    val_type* data = inStream.template read<val_type>(inReference.size());
    if (!inStream.isInDryRun())
        inReference.rebind(data);
    return inStream;
}

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_BYTESTREAM_IMPL_HPP)
