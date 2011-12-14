/* ----------------------------------------------------------------------- *//**
 *
 * @file OutputStreamBufferBase.hpp
 *
 *//* ----------------------------------------------------------------------- */


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

    /**
     * @internal One extra byte is allocated for the terminating null character.
     */
    OutputStreamBufferBase()
    :   mStorageSize(kInitialBufferSize),
        mStorage(new C[kInitialBufferSize + 1]) {
        
        setp(mStorage, mStorage + mStorageSize);
    }

    ~OutputStreamBufferBase() {
        delete mStorage;
    }
    
    /**
     * @brief Output a string
     *
     * Subclasses are required to implement this method and to feed the message
     * the the the DBMS-specific logging routine.
     * @param inMsg Null-terminated string to be output.
     * @param inLength Length of inMsg (for convenience).
     */
    void output(C* inMsg, uint32_t inLength) const {
        static_cast<const Derived*>(this)->output(inMsg, inLength);
    }

protected:
    /**
     * @brief Handle case when stream receives a character that does not fit
     *        into the current buffer any more
     *
     * This function will allocate a new buffer of twice the old buffer size. If
     * the buffer has already the maximum size kMaxBufferSize, eof is returned
     * to indicate that the buffer cannot take any more input before a flush.
     */
    int_type overflow(int_type c = traits_type::eof()) {
        if (this->pptr() >= this->epptr()) {
            if (mStorageSize >= kMaxBufferSize)
                return traits_type::eof();
            
            uint32_t newStorageSize = mStorageSize * 2;
            C* newStorage = new C[newStorageSize + 1];
            std::memcpy(newStorage, mStorage, mStorageSize);
            delete mStorage;
            mStorage = newStorage;
            
            madlib_assert(
                this->pptr() == this->epptr() &&
                this->pptr() - this->pbase() == static_cast<int64_t>(mStorageSize),
                std::logic_error("Internal error: Logging buffer has become inconsistent"));
            
            setp(mStorage, mStorage + newStorageSize);
            this->pbump(mStorageSize);
            mStorageSize = newStorageSize;
        } else if (c == traits_type::eof())
           return traits_type::eof();

        *this->pptr() = c;
        this->pbump(1);
        return traits_type::not_eof(c);
    }

    /**
     * @brief Flush and reset buffer.
     */
    int sync() {
        int length = this->pptr() - this->pbase();
        
        mStorage[length] = '\0';
        output(mStorage, length);

        setp(mStorage, mStorage + mStorageSize);
        return 0;
    }
        
    private:
        uint32_t mStorageSize;
        C* mStorage;
};
