/* ----------------------------------------------------------------------- *//**
 *
 * @file ByteString_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_BYTESTRING_PROTO_HPP
#define MADLIB_POSTGRES_BYTESTRING_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

template <class T>
class Reference;

class ByteString {
public:
    enum { isMutable = false };
    enum { kEffectiveHeaderSize
        = ((VARHDRSZ - 1) & ~(MAXIMUM_ALIGNOF - 1)) + MAXIMUM_ALIGNOF };

    ByteString(const bytea* inByteString);

    const char* ptr() const;
    size_t size() const;
    const bytea* byteString() const;
    const char& operator[](size_t inIndex) const;

protected:
    const bytea* mByteString;
};


class MutableByteString : public ByteString {
    typedef ByteString Base;

public:
    enum { isMutable = true };

    MutableByteString(bytea* inByteString);

    using Base::ptr;
    using Base::byteString;

    char* ptr();
    bytea* byteString();
    char& operator[](size_t inIndex);
};

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_BYTESTRING_PROTO_HPP)
