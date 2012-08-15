/* ----------------------------------------------------------------------- *//**
 *
 * @file ByteStreamHandleBuf_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_BYTESTREAMHANDLEBUF_PROTO_HPP
#define MADLIB_DBAL_BYTESTREAMHANDLEBUF_PROTO_HPP

namespace madlib {

namespace dbal {

template <class Storage, class CharType = typename Storage::char_type,
    bool IsMutable = Storage::isMutable>
class ByteStreamHandleBuf {
public:
    typedef Storage Storage_type;
    typedef CharType char_type;
    enum { isMutable = IsMutable };

    ByteStreamHandleBuf(size_t inSize);
    ByteStreamHandleBuf(const Storage_type& inStorage);

    // ByteStreamBuffer concept
    size_t seek(size_t inPos);
    const char_type* ptr() const;
    size_t size() const;
    size_t tell() const;

    void setStorage(Storage_type& inStorage);
    Storage_type& storage();
    const Storage_type& storage() const;

protected:
    Storage mStorage;
    size_t mPos;
};

template <class Storage, class CharType>
class ByteStreamHandleBuf<Storage, CharType, Mutable>
  : public ByteStreamHandleBuf<Storage, CharType, Immutable> {
public:
    typedef ByteStreamHandleBuf<Storage, CharType, Immutable> Base;
    typedef typename Base::Storage_type Storage_type;
    typedef typename Base::char_type char_type;
    enum { isMutable = Mutable };

    ByteStreamHandleBuf(size_t inSize);
    ByteStreamHandleBuf(const Storage_type& inStorage);

    char_type* ptr();
    void resize(size_t inSize, size_t inPivot);

    BOOST_STATIC_ASSERT_MSG(
        Storage_type::isMutable,
        "Mutable ByteStreamHandleBuf requires mutable storage.");
};

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_BYTESTREAMHANDLEBUF_PROTO_HPP)
