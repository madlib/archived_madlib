/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractOutputStreamBuffer.hpp
 *
 *//* ----------------------------------------------------------------------- */


/**
 * @brief Abstract base class for an output stream buffer
 *
 * We start out with a 1K buffer that can grow up to 16K. After that, all input
 * is ignored until the next pubsync() call. A convenient way to implicitly call
 * pubsync() is with the endl manipulator.
 *
 * Use this class by passing the pointer of an instance to the
 * ostream constructor. Example: ostream derr(new DerivedOutputStreamBuffer());
 */
template <typename _CharT = char>
class AbstractOutputStreamBuffer : public std::basic_streambuf<_CharT> {
public:
    typedef typename std::basic_streambuf<_CharT>::int_type int_type;
    typedef typename std::basic_streambuf<_CharT>::traits_type traits_type;

    static const uint32_t kInitialBufferSize = 1024;
    static const uint32_t kMaxBufferSize = 16384;

    /**
     * @internal One extra byte is allocated for the terminating null character.
     */
    AbstractOutputStreamBuffer()
    :   mStorageSize(kInitialBufferSize),
        mStorage(new _CharT[kInitialBufferSize + 1]) {
        
        setp(mStorage, mStorage + mStorageSize);
    }

    virtual ~AbstractOutputStreamBuffer() {
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
    virtual void output(_CharT *inMsg, uint32_t inLength) = 0;

protected:
    virtual int_type overflow(int_type c = traits_type::eof()) {
        if (this->pptr() >= this->epptr()) {
            if (mStorageSize >= kMaxBufferSize)
                return traits_type::eof();
            
            uint32_t newStorageSize = mStorageSize * 2;
            _CharT *newStorage = new _CharT[newStorageSize + 1];
            std::memcpy(newStorage, mStorage, mStorageSize);
            delete mStorage;
            mStorage = newStorage;
            
            BOOST_ASSERT_MSG(
                this->pptr() == this->epptr() &&
                this->pptr() - this->pbase() == mStorageSize,
                "Internal error: Logging buffer has become inconsistent");
            
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
    virtual int sync() {
        int length = this->pptr() - this->pbase();
        
        mStorage[length] = '\0';
        output(mStorage, length);

        setp(mStorage, mStorage + mStorageSize);
        return 0;
    }
        
    private:
        uint32_t mStorageSize;
        _CharT *mStorage;
};
