/* ----------------------------------------------------------------------- *//**
 *
 * @file ByteStream_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_BYTESTREAM_PROTO_HPP
#define MADLIB_DBAL_BYTESTREAM_PROTO_HPP

namespace madlib {

namespace dbal {

struct ByteStreamMaximumAlignmentType {
};

template <class T>
struct ByteStreamDefaultTypeTraits {
    enum { alignment = sizeof(T) <= 16 ? sizeof(T) : 16 };
};

// Specialize for the maximum-alignment dummy type. There is no supported
// architecture at the moment for which 16 bytes would not be enough.
template <>
struct ByteStreamDefaultTypeTraits<ByteStreamMaximumAlignmentType> {
    enum { alignment = 16 };
};

template <
    class StreamBuf,
    template <class T> class TypeTraits = ByteStreamDefaultTypeTraits,
    bool IsMutable = StreamBuf::isMutable>
class ByteStream {
public:
    typedef StreamBuf StreamBuf_type;
    typedef typename StreamBuf_type::char_type char_type;
    enum { isMutable = IsMutable };
    enum { maximumAlignment =
        TypeTraits<ByteStreamMaximumAlignmentType>::alignment };

    friend class DryRun;

    class DryRun {
    public:
        DryRun(ByteStream& inStream);
        ~DryRun();
        void leave();

    protected:
        ByteStream& mStream;
        bool mIsIn;
    };

    ByteStream(StreamBuf_type* inStreamBuf);

    template <size_t Alignment> size_t seek(std::ptrdiff_t inOffset,
        std::ios_base::seekdir inDir);
    size_t seek(size_t inPos);
    size_t seek(std::ptrdiff_t inOffset, std::ios_base::seekdir inDir);
    size_t available() const;
    const char_type* ptr() const;
    size_t size() const;
    size_t tell() const;
    std::ios_base::iostate rdstate() const;
    bool eof() const;
    bool isInDryRun() const;
    template <class T> const T* read(size_t inCount = 1);

protected:
    void enterDryRun();
    void leaveDryRun();

    /**
     * @brief The associated storage to the stream (similar to a streambuf for
     *     IOStreams)
     */
    StreamBuf_type* mStreamBuf;

    /**
     * @brief In dry mode read/write operations will only move the cursor, but
     *     other objects are not touched
     */
    int32_t mDryRun;
};

template <class StreamBuf, template <class T> class TypeTraits>
class ByteStream<StreamBuf, TypeTraits, Mutable>
  : public ByteStream<StreamBuf, TypeTraits, Immutable> {
public:
    typedef ByteStream<StreamBuf, TypeTraits, Immutable> Base;
    typedef typename Base::StreamBuf_type StreamBuf_type;
    typedef typename Base::char_type char_type;

    ByteStream(StreamBuf_type* inStreamBuf);
    char_type* ptr();
    template <class T> T* read(size_t inCount = 1);
};

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_BYTESTREAM_PROTO_HPP)
