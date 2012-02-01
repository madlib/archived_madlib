/* ----------------------------------------------------------------------- *//**
 *
 * @file OutputStreamBufferBase_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by EigenLinAlgTypes.hpp
#ifdef MADLIB_DBAL_HPP

/**
 * @brief Base class for an output stream buffer
 *
 * We start out with a 1K buffer that can grow up to 16K. After that, all input
 * is ignored until the next pubsync() call. A convenient way to implicitly call
 * pubsync() is with the endl manipulator.
 *
 * Use this class by passing the pointer of an instance to the
 * ostream constructor. Example: ostream derr(new DerivedOutputStreamBuffer());
 */
template <class Derived, typename C = char>
class OutputStreamBufferBase : public std::basic_streambuf<C> {
public:
    typedef typename std::basic_streambuf<C>::int_type int_type;
    typedef typename std::basic_streambuf<C>::traits_type traits_type;

    static const uint32_t kInitialBufferSize = 1024;
    static const uint32_t kMaxBufferSize = 16384;

    OutputStreamBufferBase();
    ~OutputStreamBufferBase();
    void output(C* inMsg, uint32_t inLength) const;

protected:
    int_type overflow(int_type c = traits_type::eof());
    int sync();
        
private:
    uint32_t mStorageSize;
    C* mStorage;
};

#endif // MADLIB_DBAL_HPP (workaround for Doxygen)
