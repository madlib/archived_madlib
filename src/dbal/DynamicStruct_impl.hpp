/* ----------------------------------------------------------------------- *//**
 *
 * @file DynamicStruct_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_DYNAMICSTRUCT_IMPL_HPP
#define MADLIB_POSTGRES_DYNAMICSTRUCT_IMPL_HPP

namespace madlib {

namespace dbal {

/**
 * @brief Meta function for mapping types to the dynamic-struct mapped type
 *
 * @tparam T Desired type that is to be stored in the dynamic struct
 * @tparam IsMutable Indicator if the dynamic struct is mutable
 */
template <class T, bool IsMutable>
struct DynamicStructType {
    typedef Ref<T, IsMutable> type;
};

template <bool IsMutable>
struct DynamicStructType<eigen_integration::ColumnVector, IsMutable> {
    typedef eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            eigen_integration::ColumnVector,
            const eigen_integration::ColumnVector>::type,
        TransparentHandle<double, IsMutable> > type;
};

template <bool IsMutable>
struct DynamicStructType<eigen_integration::Matrix, IsMutable> {
    typedef eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            eigen_integration::Matrix,
            const eigen_integration::Matrix>::type,
        TransparentHandle<double, IsMutable> > type;
};

// DynamicStructRootContainer<Storage, TypeTraits>

template <class Storage, template <class T> class TypeTraits>
DynamicStructRootContainer<Storage, TypeTraits>::DynamicStructRootContainer(
    const DynamicStructRootContainer<Storage, TypeTraits>::Storage_type&
        inStorage)
  : mByteStreamBuf(inStorage) { }

template <class Storage, template <class T> class TypeTraits>
const typename DynamicStructRootContainer<Storage, TypeTraits>::StreamBuf_type&
DynamicStructRootContainer<Storage, TypeTraits>::streambuf() const {
    return mByteStreamBuf;
}

template <class Storage, template <class T> class TypeTraits>
typename DynamicStructRootContainer<Storage, TypeTraits>::StreamBuf_type&
DynamicStructRootContainer<Storage, TypeTraits>::streambuf() {
    return
        const_cast<StreamBuf_type&>(
            static_cast<const DynamicStructRootContainer*>(this)->streambuf()
        );
}


// DynamicStructBase<Derived, Container, IsMutable>

template <class Derived, class Container, bool IsMutable>
inline
DynamicStructBase<Derived, Container, IsMutable>::DynamicStructBase(
    Init_type& inContainer)
  : mContainer(inContainer) { }


template <class Derived, class Container, bool IsMutable>
inline
const typename DynamicStructBase<Derived, Container, IsMutable>
    ::RootContainer_type&
DynamicStructBase<Derived, Container, IsMutable>::rootContainer() const {
    return mContainer.rootContainer();
}

template <class Derived, class Container, bool IsMutable>
void
DynamicStructBase<Derived, Container, IsMutable>::initialize() { }


template <class Derived, bool IsMutable, class Storage,
    template <class T> class TypeTraits>
inline
DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, IsMutable>
    ::DynamicStructBase(Init_type& inStorage)
  : mContainer(inStorage), mByteStream(&mContainer.streambuf()) { }

template <class Derived, bool IsMutable, class Storage,
    template <class T> class TypeTraits>
inline
const typename DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>,
    IsMutable>::RootContainer_type&
DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, IsMutable>
    ::rootContainer() const {

    return static_cast<RootContainer_type&>(*this);
}

template <class Derived, bool IsMutable, class Storage, template <class T> class TypeTraits>
inline
const typename DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>,
    IsMutable>::Storage_type&
DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, IsMutable>
    ::storage() const {

    return mContainer.streambuf().storage();
}

template <class Derived, bool IsMutable, class Storage,
    template <class T> class TypeTraits>
inline
const typename DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, IsMutable>
    ::ByteStream_type&
DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, IsMutable>::byteStream()
    const {

    return mByteStream;
}


// DynamicStructBase<Derived, Container, Mutable>

template <class Derived, class Container>
template <class SubStruct>
void
DynamicStructBase<Derived, Container, Mutable>::setSize(
    SubStruct& inSubStruct, size_t inSize) {

    Base::mContainer.setSize(inSubStruct, inSize);
}

/**
 * @brief Change the size of a sub-struct
 */
template <class Derived, class Storage, template <class T> class TypeTraits>
template <class SubStruct>
inline
void
DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, Mutable>::setSize(
    SubStruct& inSubStruct, size_t inSize) {

    if (inSubStruct.size() == inSize)
        return;

    typename Container_type::StreamBuf_type& streamBuf
        = mContainer.streambuf();
    streamBuf.resize(streamBuf.size() + inSize - inSubStruct.size(),
        inSubStruct.end());
    mByteStream.seek(0, std::ios_base::beg);
    mByteStream >> static_cast<Derived&>(*this);

    if (mByteStream.eof())
        throw std::runtime_error("Out-of-bounds byte-string access "
            "detected during resize.");
}


// DynamicStructBase<Derived, DynamicStructRootContainer<Storage, TypeTraits>,
//     IsMutable>

template <class Derived, bool IsMutable, class Storage,
    template <class T> class TypeTraits>
inline
void
DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, IsMutable>::initialize() {

    mByteStream.seek(0, std::ios_base::beg);
    mByteStream >> static_cast<Derived&>(*this);

    if (mByteStream.eof()) {
        // The assumption is that either
        // a) we have a valid dynamic struct, in which case we do not end here
        // b) we have an unintialized dynamic struct, which only consists of
        //    (too few) zero bytes.
        // If (b) is violated, then mByteStream.tell() might not have the
        // correct size information.

        typedef typename Container_type::StreamBuf_type StreamBuf_type;

        StreamBuf_type& streamBuf = mContainer.streambuf();
        streamBuf = StreamBuf_type(mByteStream.tell());
        mByteStream.seek(0, std::ios_base::beg);
        mByteStream >> static_cast<Derived&>(*this);

        if (mByteStream.eof())
            throw std::runtime_error("Out-of-bounds byte-string access "
                "detected during initialization of mutable dynamic struct.");
    }
}


// DynamicStructBase<Derived, DynamicStructRootContainer<Storage, TypeTraits>,
//     Mutable>

/* FIXME: Remove this.

template <class Derived, class Storage, template <class T> class TypeTraits>
inline
void
DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, Mutable>::initialize() {

    mByteStream.seek(0, std::ios_base::beg);
    mByteStream >> static_cast<Derived&>(*this);
    if (mByteStream.eof()) {
        typename Container_type::StreamBuf_type& streamBuf
            = mContainer.streambuf();
        streamBuf.resize(mByteStream.tell(), streamBuf.size());
        mByteStream.seek(0, std::ios_base::beg);
        mByteStream >> static_cast<Derived&>(*this);

        if (mByteStream.eof())
            throw std::runtime_error("Out-of-bounds byte-string access "
                "detected during initialization of mutable dynamic struct.");
    }
}
*/

// DynamicStruct<Derived, Container, IsMutable>

template <class Derived, class Container, bool IsMutable>
inline
DynamicStruct<Derived, Container, IsMutable>::DynamicStruct(Init_type& inInitialization)
  : Base(inInitialization) { }

template <class Derived, class Container, bool IsMutable>
inline
typename DynamicStruct<Derived, Container, IsMutable>::RootContainer_type&
DynamicStruct<Derived, Container, IsMutable>::rootContainer() {
    return const_cast<RootContainer_type&>(
        static_cast<const DynamicStruct*>(this)->rootContainer()
    );
}

template <class Derived, class Container, bool IsMutable>
inline
typename DynamicStruct<Derived, Container, IsMutable>::Storage_type&
DynamicStruct<Derived, Container, IsMutable>::storage() {
    return const_cast<Storage_type&>(
        static_cast<const DynamicStruct*>(this)->storage()
    );
}

template <class Derived, class Container, bool IsMutable>
inline
typename DynamicStruct<Derived, Container, IsMutable>::ByteStream_type&
DynamicStruct<Derived, Container, IsMutable>::byteStream() {
    return const_cast<ByteStream_type&>(
        static_cast<const DynamicStruct*>(this)->byteStream()
    );
}

template <class Derived, class Container, bool IsMutable>
inline
size_t
DynamicStruct<Derived, Container, IsMutable>::begin() const {
    return mBegin;
}

template <class Derived, class Container, bool IsMutable>
inline
size_t
DynamicStruct<Derived, Container, IsMutable>::end() const {
    return mEnd;
}

template <class Derived, class Container, bool IsMutable>
inline
typename DynamicStruct<Derived, Container, IsMutable>::char_type*
DynamicStruct<Derived, Container, IsMutable>::ptr() {
    return const_cast<char_type*>(
        static_cast<const DynamicStruct*>(this)->ptr()
    );
}

template <class Derived, class Container, bool IsMutable>
inline
const typename DynamicStruct<Derived, Container, IsMutable>::char_type*
DynamicStruct<Derived, Container, IsMutable>::ptr() const {
    return this->storage().ptr() + begin();
}

template <class Derived, class Container, bool IsMutable>
inline
size_t
DynamicStruct<Derived, Container, IsMutable>::size() const {
    return end() - begin();
}

/*
 * Note that there is also a version for
 * DynamicStruct<Derived, Container, Mutable>!
 */
template <class Derived, class Container, bool IsMutable>
inline
void
DynamicStruct<Derived, Container, IsMutable>::bindToStream(
    typename DynamicStruct<Derived, Container, IsMutable>::ByteStream_type&
        inStream) {

    inStream.template seek<ByteStream_type::maximumAlignment>(0,
        std::ios_base::cur);

    if (!inStream.isInDryRun())
        this->mBegin = inStream.tell();

    static_cast<Derived*>(this)->bind(inStream);
    inStream.template seek<ByteStream_type::maximumAlignment>(0,
        std::ios_base::cur);

    if (!inStream.isInDryRun())
        this->mEnd = inStream.tell();
}


// DynamicStruct<Derived, Container, Mutable>

template <class Derived, class Container>
inline
DynamicStruct<Derived, Container, Mutable>::DynamicStruct(
    Init_type& inInitialization)
  : Base(inInitialization), mSizeIsLocked(false) { }


/**
 * @brief Internal function to change size.
 *
 * This assumes that \c inSize is the correct size!
 */
template <class Derived, class Container>
inline
void
DynamicStruct<Derived, Container, Mutable>::setSize(size_t inSize) {
    this->setSize(static_cast<Derived&>(*this), inSize);
    Base::mEnd = Base::mBegin + inSize;
}

template <class Derived, class Container>
inline
void
DynamicStruct<Derived, Container, Mutable>::resize() {
    size_t begin = this->begin();
    ByteStream_type& stream = this->byteStream();
    stream.seek(begin, std::ios_base::beg);

    // We use an RAII object here that ensure that dry mode will also be
    // left in case of exceptions
    typename ByteStream_type::DryRun dryRun(stream);
    stream >> static_cast<Derived&>(*this);
    dryRun.leave();

    stream.template seek<ByteStream_type::maximumAlignment>(0,
        std::ios_base::cur);
    size_t newEnd = stream.tell();
    this->setSize(newEnd - begin);
}

template <class Derived, class Container>
template <class OtherDerived>
DynamicStruct<Derived, Container, Mutable>&
DynamicStruct<Derived, Container, Mutable>::copy(
    const DynamicStruct<OtherDerived, typename OtherDerived::Container_type>&
        inOtherStruct) {

    if (this->size() != inOtherStruct.size()) {
        this->setSize(inOtherStruct.size());
        mSizeIsLocked = true;
    }

    // We now have enough space to copy everything from inOtherStruct
    std::copy(inOtherStruct.ptr(), inOtherStruct.ptr() + this->size(),
        this->ptr());

    mSizeIsLocked = false;
    this->resize();
    return *this;
}

/*
 * This method is overridden to take special care of the case when
 * <tt>mSizeIsLocked == true</tt>.
 */
template <class Derived, class Container>
inline
void
DynamicStruct<Derived, Container, Mutable>::bindToStream(
    typename DynamicStruct<Derived, Container, Mutable>::ByteStream_type&
        inStream) {

    inStream.template seek<ByteStream_type::maximumAlignment>(0,
        std::ios_base::cur);

    size_t begin = inStream.tell();
    size_t size = this->size();

    if (!inStream.isInDryRun())
        this->mBegin = begin;

    static_cast<Derived*>(this)->bind(inStream);

    if (mSizeIsLocked)
        inStream.template seek(begin + size, std::ios_base::beg);
    else
        inStream.template seek<ByteStream_type::maximumAlignment>(0,
            std::ios_base::cur);

    if (!inStream.isInDryRun())
        this->mEnd = inStream.tell();
}

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_DYNAMICSTRUCT_IMPL_HPP)
