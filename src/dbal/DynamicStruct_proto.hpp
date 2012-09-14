/* ----------------------------------------------------------------------- *//**
 *
 * @file DynamicStruct_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_DYNAMICSTRUCT_PROTO_HPP
#define MADLIB_DBAL_DYNAMICSTRUCT_PROTO_HPP

namespace madlib {

namespace dbal {

template <
    class Storage,
    template <class T> class TypeTraits>
class DynamicStructRootContainer {
public:
    typedef ByteStreamHandleBuf<Storage> StreamBuf_type;
    typedef typename StreamBuf_type::Storage_type Storage_type;
    typedef ByteStream<StreamBuf_type, TypeTraits> ByteStream_type;
    enum { isMutable = Storage_type::isMutable };

    DynamicStructRootContainer(const Storage_type& inStorage);
    const StreamBuf_type& streambuf() const;
    StreamBuf_type& streambuf();

protected:
    StreamBuf_type mByteStreamBuf;
};

template <class Derived, class Container, bool IsMutable = Container::isMutable>
class DynamicStructBase {
public:
    typedef Container Container_type;
    typedef typename Container_type::RootContainer_type RootContainer_type;
    typedef typename Container_type::Storage_type Storage_type;
    typedef typename Container_type::ByteStream_type ByteStream_type;
    typedef Container_type Init_type;

    DynamicStructBase(Init_type& inContainer);
    void initialize();
    void rebindAll();
    const RootContainer_type& rootContainer() const;

protected:
    Container_type& mContainer;
};

template <class Derived, class Container>
class DynamicStructBase<Derived, Container, Mutable>
  : public DynamicStructBase<Derived, Container, Immutable> {
public:
    typedef DynamicStructBase<Derived, Container, Immutable> Base;
    typedef typename Base::Init_type Init_type;

    DynamicStructBase(Init_type& inContainer);

protected:
    template <class SubStruct> void setSize(SubStruct &inSubStruct, size_t inSize);

    using Base::mContainer;
};

template <
    class Derived,
    bool IsMutable,
    class Storage,
    template <class T> class TypeTraits>
class DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, IsMutable> {
public:
    typedef DynamicStructRootContainer<Storage, TypeTraits> Container_type;
    typedef Derived RootContainer_type;
    typedef typename Container_type::Storage_type Storage_type;
    typedef typename Container_type::ByteStream_type ByteStream_type;
    typedef const Storage_type Init_type;

    DynamicStructBase(Init_type& inStorage);
    void initialize();
    void rebindAll();
    const RootContainer_type& rootContainer() const;
    const Storage_type& storage() const;
    const ByteStream_type& byteStream() const;

protected:
    Container_type mContainer;
    ByteStream_type mByteStream;
};

template <
    class Derived,
    class Storage,
    template <class T> class TypeTraits>
class DynamicStructBase<Derived,
    DynamicStructRootContainer<Storage, TypeTraits>, Mutable>
  : public DynamicStructBase<Derived,
        DynamicStructRootContainer<Storage, TypeTraits>, Immutable> {

public:
    typedef DynamicStructBase<Derived,
        DynamicStructRootContainer<Storage, TypeTraits>, Immutable> Base;
    typedef typename Base::Container_type Container_type;
    typedef typename Base::Init_type Init_type;

    DynamicStructBase(Init_type& inStorage) : Base(inStorage) { }
//    void initialize();

protected:
    template <class SubStruct> void setSize(SubStruct &inSubStruct,
        size_t inSize);

    using Base::mContainer;
    using Base::mByteStream;
};


template <class Derived, class Container, bool IsMutable = Container::isMutable>
class DynamicStruct
  : public DynamicStructBase<Derived, Container, Container::isMutable> {

public:
    typedef DynamicStructBase<Derived, Container> Base;
    typedef typename Base::Storage_type Storage_type;
    typedef typename Base::Container_type Container_type;
    typedef typename Base::RootContainer_type RootContainer_type;
    typedef typename Base::ByteStream_type ByteStream_type;
    typedef typename Base::Init_type Init_type;
    typedef typename ByteStream_type::char_type char_type;
    enum { isMutable = Container_type::isMutable };

    DynamicStruct(Init_type& inInitialization);

    using Base::rootContainer;
    RootContainer_type& rootContainer();
    using Base::storage;
    Storage_type& storage();
    using Base::byteStream;
    ByteStream_type& byteStream();

    size_t begin() const;
    size_t end() const;
    const char_type* ptr() const;
    char_type* ptr();
    size_t size() const;

    void bindToStream(ByteStream_type& inStream);

    /**
     * @brief Bind a DynamicStruct object to the current position in the stream
     *
     * The following idiom (in-class friend function together with static
     * polymorphicsm) is called the "Barton-Nackman trick". Essentially, we only
     * make operator>> visible if type Derived is involved.
     *
     * Subclasses have to implement the bind() function.
     */
    friend
    ByteStream_type&
    operator>>(ByteStream_type& inStream, Derived& inStruct) {
        inStruct.bindToStream(inStream);
        return inStream;
    }

protected:
    size_t mBegin;
    size_t mEnd;
};

template <class Derived, class Container>
class DynamicStruct<Derived, Container, Mutable>
  : public DynamicStruct<Derived, Container, Immutable> {

public:
    typedef DynamicStruct<Derived, Container, Immutable> Base;
    typedef typename Base::Init_type Init_type;
    typedef typename Base::ByteStream_type ByteStream_type;

    DynamicStruct(Init_type& inInitialization);

    using Base::setSize;
    void setSize(size_t inSize);
    void resize();

    void bindToStream(ByteStream_type& inStream);

protected:
    bool mSizeIsLocked;

    template <class OtherDerived> DynamicStruct& copy(
        const DynamicStruct<OtherDerived, typename OtherDerived::Container_type>&
            inOtherStruct);
};

template <typename T, bool IsMutable>
class Ref;

namespace eigen_integration {

template <class EigenType, class Handle, int MapOptions>
class HandleMap;

}

// Because of its dependencies on Eigen, DynamicStructType is only declared in
// DynamicStruct_impl.hpp.
template <class T, bool IsMutable>
struct DynamicStructType;

#define MADLIB_DYNAMIC_STRUCT_TYPEDEFS \
    typedef typename Base::Init_type Init_type; \
    typedef typename Base::ByteStream_type ByteStream_type; \
    typedef typename Base::Storage_type Storage_type; \
    typedef typename DynamicStructType<double, Base::isMutable>::type double_type; \
    typedef typename DynamicStructType<float, Base::isMutable>::type float_type; \
    typedef typename DynamicStructType<uint64_t, Base::isMutable>::type uint64_type; \
    typedef typename DynamicStructType<int64_t, Base::isMutable>::type int64_type; \
    typedef typename DynamicStructType<uint32_t, Base::isMutable>::type uint32_type; \
    typedef typename DynamicStructType<int32_t, Base::isMutable>::type int32_type; \
    typedef typename DynamicStructType<uint16_t, Base::isMutable>::type uint16_type; \
    typedef typename DynamicStructType<int16_t, Base::isMutable>::type int16_type; \
    typedef typename DynamicStructType<ColumnVector, Base::isMutable>::type \
        ColumnVector_type; \
    typedef typename DynamicStructType<Matrix, Base::isMutable>::type Matrix_type; \
    enum { isMutable = Base::isMutable }


} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_DYNAMICSTRUCT_PROTO_HPP)
