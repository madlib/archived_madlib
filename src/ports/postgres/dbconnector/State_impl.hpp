/* ----------------------------------------------------------------------- *//**
 *
 * @file State_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_STATE_IMPL_HPP
#define MADLIB_POSTGRES_STATE_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

template <class Storage>
struct StorageTraits<Storage, false /* IsMutable */> {
    enum { isMutable = false };
    typedef const char char_type;
    typedef const double double_type;
    typedef const uint64_t uint64_type;
    typedef const uint32_t uint32_type;
    typedef const uint16_t uint16_type;
    typedef dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::ColumnVector,
        TransparentHandle<double>
    > MappedColumnVector_type;
    typedef dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::Matrix,
        TransparentHandle<double>
    > MappedMatrix_type;
};

template <class Storage>
struct StorageTraits<Storage, true /* IsMutable */> {
    enum { isMutable = true };
    typedef char char_type;
    typedef double double_type;
    typedef uint64_t uint64_type;
    typedef uint32_t uint32_type;
    typedef uint16_t uint16_type;
    typedef dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::ColumnVector,
        MutableTransparentHandle<double>
    > MappedColumnVector_type;
    typedef dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::Matrix,
        MutableTransparentHandle<double>
    > MappedMatrix_type;
};

template <class StreamBuf>
template <class T>
inline
T*
BinaryStream<StreamBuf>::read(size_t inCount) {
    this->seek<TypeTraits<typename boost::remove_cv<T>::type>::alignment>(
        0, std::ios_base::cur);
    T* pointer = this->available() >= inCount * sizeof(T)
        ? reinterpret_cast<T*>(this->ptr() + this->tell())
        : NULL;
    this->seek(inCount * sizeof(T), std::ios_base::cur);
    return pointer;
}

template <class StreamBuf>
template <std::size_t Alignment>
inline
size_t
BinaryStream<StreamBuf>::seek(std::ptrdiff_t inOffset,
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

    newPos += inOffset;
    newPos = ((newPos - 1) & ~(Alignment - 1)) + Alignment;

    return this->seek(newPos);
}

template <class StreamBuf>
inline
size_t
BinaryStream<StreamBuf>::seek(std::ptrdiff_t inOffset,
    std::ios_base::seekdir inDir) {

    return seek<1>(inOffset, inDir);
}

template <class StreamBuf>
inline
std::ios_base::iostate
BinaryStream<StreamBuf>::rdstate() const {
    return (this->tell() <= this->size()
        ? std::ios_base::goodbit
        : std::ios_base::eofbit);
}

template <class StreamBuf>
inline
bool
BinaryStream<StreamBuf>::eof() const {
    return rdstate() & std::ios_base::eofbit;
}


template <class StreamBuf>
inline
size_t
BinaryStream<StreamBuf>::available() const {
    if (this->size() < this->tell())
        return 0;
    else
        return this->size() - this->tell();
}

template <class StreamBuf>
inline
size_t
BinaryStream<StreamBuf>::tell() const {
    return mStreamBuf->tell();
}

template <class StreamBuf, typename T>
BinaryStream<StreamBuf>&
operator>>(
    BinaryStream<StreamBuf>& inStream,
    Ref<T>& inReference) {

    T* data = inStream.template read<T>();
    if (!inStream.isInDryMode())
        inReference.rebind(data);
    return inStream;
}

/*template <class T, bool Immutable>
struct add_const_if_immutable {
    typedef T type;
};

template <class T>
struct add_const_if_immutable<T, true> {
    typedef const T type;
};*/


template <class StreamBuf, typename EigenType>
BinaryStream<StreamBuf>&
operator>>(
    BinaryStream<StreamBuf>& inStream,
    dbal::eigen_integration::HandleMap<
        EigenType,
        TransparentHandle<typename EigenType::Scalar>
    >& inReference) {

    typedef BinaryStream<StreamBuf> BinaryStream_type;

//    typename add_const_if_immutable<
//                typename EigenType::Scalar,
//                !BinaryStream_type::isMutable
//            >::type
    const typename EigenType::Scalar* data
        = inStream.template read<const typename EigenType::Scalar>(
            inReference.size());
    if (!inStream.isInDryMode())
        inReference.rebind(data);
    return inStream;
}

template <class StreamBuf, typename EigenType>
BinaryStream<StreamBuf>&
operator>>(
    BinaryStream<StreamBuf>& inStream,
    dbal::eigen_integration::HandleMap<
        EigenType,
        MutableTransparentHandle<typename EigenType::Scalar>
    >& inReference) {

    typename EigenType::Scalar* data
        = inStream.template read<typename EigenType::Scalar>(
            inReference.size());
    if (!inStream.isInDryMode())
        inReference.rebind(data);
    return inStream;
}

template <class Derived, class Parent, bool IsMutable>
inline
StateBase<Derived, Parent, IsMutable>::StateBase(Init_type& inParent)
  : mParent(inParent) { }


//template <class Derived, class Parent, bool IsMutable>
//inline
//const typename StateBase<Derived, Parent, IsMutable>::BinaryStream_type&
//StateBase<Derived, Parent, IsMutable>::binaryStream() const {
//    return mParent.binaryStream();
//}

template <class Derived, class Parent, bool IsMutable>
inline
const typename StateBase<Derived, Parent, IsMutable>::RootState_type&
StateBase<Derived, Parent, IsMutable>::rootState() const {
    return mParent.rootState();
}

//template <class Derived, class Parent, bool IsMutable>
//inline
//const typename StateBase<Derived, Parent, IsMutable>::Storage_type&
//StateBase<Derived, Parent, IsMutable>::storage() const {
//    return mParent.storage();
//}

template <class Derived, class Parent>
template <class SubState>
void
StateBase<Derived, Parent, true /* IsMutable */>::setSize(
    SubState& inSubState, size_t inSize) {

    Base::mParent.setSize(inSubState, inSize);
}

template <class Derived, class Parent, bool IsMutable>
void
StateBase<Derived, Parent, IsMutable>::initialize() { }


template <class Derived, bool IsMutable, class Storage>
inline
StateBase<Derived, RootState<Storage>, IsMutable>::StateBase(
    Init_type& inStorage)
  : mParent(inStorage), mBinaryStream(&mParent) { }

template <class Derived, bool IsMutable, class Storage>
inline
const typename StateBase<Derived, RootState<Storage>, IsMutable>
    ::RootState_type&
StateBase<Derived, RootState<Storage>, IsMutable>::rootState() const {
    return static_cast<RootState_type&>(*this);
}

template <class Derived, bool IsMutable, class Storage>
inline
const typename StateBase<Derived, RootState<Storage>, IsMutable>
    ::Storage_type&
StateBase<Derived, RootState<Storage>, IsMutable>::storage() const {
    return mParent.storage();
}

template <class Derived, bool IsMutable, class Storage>
inline
const typename StateBase<Derived, RootState<Storage>, IsMutable>
    ::BinaryStream_type&
StateBase<Derived, RootState<Storage>, IsMutable>::binaryStream()
    const {

    return mBinaryStream;
}

template <class Derived, class Storage>
template <class SubState>
inline
void
StateBase<Derived, RootState<Storage>, true /* IsMutable */>::setSize(
    SubState& inSubState, size_t inSize) {

    if (inSubState.size() == inSize)
        return;

    mParent.resize(mParent.size() + inSize - inSubState.size(),
        inSubState.end());
    mBinaryStream.seek(0, std::ios_base::beg);
    mBinaryStream >> static_cast<Derived&>(*this);

    if (mBinaryStream.eof())
        throw std::runtime_error("Out-of-bounds byte-string access "
            "detected during resize.");
}

template <class Derived, bool IsMutable, class Storage>
inline
void
StateBase<Derived, RootState<Storage>, IsMutable>
    ::initialize() {

    mBinaryStream >> static_cast<Derived&>(*this);
    if (mBinaryStream.eof())
        throw std::runtime_error("Out-of-bounds byte-string access "
            "detected during initialization of immutable state.");
}

template <class Derived, class Storage>
inline
void
StateBase<Derived, RootState<Storage>, true /* IsMutable */>
    ::initialize() {

    mBinaryStream >> static_cast<Derived&>(*this);
    if (mBinaryStream.eof()) {
        mParent.resize(mBinaryStream.tell(), mParent.size());
        mBinaryStream.seek(0, std::ios_base::beg);
        mBinaryStream >> static_cast<Derived&>(*this);

        if (mBinaryStream.eof())
            throw std::runtime_error("Out-of-bounds byte-string access "
                "detected during initialization of mutable stabe.");
    }
}

template <class Derived, class Parent, bool IsMutable>
inline
State<Derived, Parent, IsMutable>::State(Init_type& inInitialization)
  : Base(inInitialization) { }

template <class Derived, class Parent, bool IsMutable>
inline
typename State<Derived, Parent, IsMutable>::RootState_type&
State<Derived, Parent, IsMutable>::rootState() {
    return const_cast<RootState_type&>(
        static_cast<const State*>(this)->rootState()
    );
}

template <class Derived, class Parent, bool IsMutable>
inline
typename State<Derived, Parent, IsMutable>::Storage_type&
State<Derived, Parent, IsMutable>::storage() {
    return const_cast<Storage_type&>(
        static_cast<const State*>(this)->storage()
    );
}

template <class Derived, class Parent, bool IsMutable>
inline
typename State<Derived, Parent, IsMutable>::BinaryStream_type&
State<Derived, Parent, IsMutable>::binaryStream() {
    return const_cast<BinaryStream_type&>(
        static_cast<const State*>(this)->binaryStream()
    );
}

template <class Derived, class Parent>
inline
void
State<Derived, Parent, true /* IsMutable */>::setSize(size_t inSize) {
    this->setSize(static_cast<Derived&>(*this), inSize);
}

template <class Derived, class Parent, bool IsMutable>
inline
size_t
State<Derived, Parent, IsMutable>::begin() const {
    return mBegin;
}

template <class Derived, class Parent, bool IsMutable>
inline
size_t
State<Derived, Parent, IsMutable>::end() const {
    return mEnd;
}

template <class Derived, class Parent, bool IsMutable>
inline
typename State<Derived, Parent, IsMutable>::char_type*
State<Derived, Parent, IsMutable>::ptr() {
    return const_cast<char_type*>(
        static_cast<const State*>(this)->ptr()
    );
}

template <class Derived, class Parent, bool IsMutable>
inline
const typename State<Derived, Parent, IsMutable>::char_type*
State<Derived, Parent, IsMutable>::ptr() const {
    return this->storage().ptr() + begin();
}

template <class Derived, class Parent, bool IsMutable>
inline
size_t
State<Derived, Parent, IsMutable>::size() const {
    return end() - begin();
}


template <class Derived, class Parent>
inline
void
State<Derived, Parent, true /* IsMutable */>::resize() {
    size_t begin = this->begin();
    BinaryStream_type& stream = this->binaryStream();
    stream.seek(begin, std::ios_base::beg);

    // We use an RAII object here that ensure that dry mode will also be
    // left in case of exceptions
    typename BinaryStream_type::DryMode dryMode(stream);
    stream >> static_cast<Derived&>(*this);
    dryMode.leave();

    stream.template seek<MAXIMUM_ALIGNOF>(0, std::ios_base::cur);
    size_t newEnd = stream.tell();
    this->setSize(newEnd - begin);
}

template <class Derived, class Parent>
template <class OtherDerived>
State<Derived, Parent, true /* IsMutable */>&
State<Derived, Parent, true /* IsMutable */>::copy(
    const State<OtherDerived, typename OtherDerived::Parent_type>&
        inOtherState) {

    if (this->size() != inOtherState.size())
        this->setSize(inOtherState.size());

    std::copy(inOtherState.ptr(), inOtherState.ptr() + this->size(),
        this->ptr());
    this->resize();
    return *this;
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_STATE_IMPL_HPP)
