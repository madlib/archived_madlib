/* ----------------------------------------------------------------------- *//**
 *
 * @file ByteString_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_BYTESTRING_IMPL_HPP
#define MADLIB_POSTGRES_BYTESTRING_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

inline
ByteString::ByteString(const bytea* inByteString)
  : mByteString(inByteString) { }

inline
const ByteString::char_type*
ByteString::ptr() const {
    return reinterpret_cast<const char_type*>(mByteString)
        + kEffectiveHeaderSize;
}

inline
size_t
ByteString::size() const {
    return VARSIZE(mByteString) < kEffectiveHeaderSize
        ? 0
        : VARSIZE(mByteString) - kEffectiveHeaderSize;
}

inline
const bytea*
ByteString::byteString() const {
    return mByteString;
}

inline
const ByteString::char_type&
ByteString::operator[](size_t inIndex) const {
    madlib_assert(inIndex < size(), std::runtime_error(
        "Out-of-bounds byte-string access detected."));

    return ptr()[inIndex];
}

inline
MutableByteString::MutableByteString(bytea* inByteString)
  : ByteString(inByteString) { }

inline
ByteString::char_type*
MutableByteString::ptr() {
    return const_cast<char_type*>(static_cast<const ByteString*>(this)->ptr());
}

inline
bytea*
MutableByteString::byteString() {
    return const_cast<bytea*>(Base::mByteString);
}

inline
ByteString::char_type&
MutableByteString::operator[](size_t inIndex) {
    return const_cast<char_type&>(
        static_cast<const ByteString*>(this)->operator[](inIndex)
    );
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_ANYTYPE_IMPL_HPP)
