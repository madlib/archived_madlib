/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractDBInterface.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Abstract base class for the interface to a DBMS backend
 *
 * This class provides the interface that MADlib modules use for communicating
 * wit the DBMS backend. Every MADlib module function is passed a reference to
 * an AbstractDBInterface object as first argument.
 */
class AbstractDBInterface {
protected:
    /**
     * @brief Differentiate between Armadillo warnings and errors.
     *
     * Armadillo only uses one output stream. This requires us to provide some
     * additional checks for disptaching messages correctly.
     */
    class ArmadilloOutputStreamBuffer : public AbstractOutputStreamBuffer<> {
    public:
        ArmadilloOutputStreamBuffer(
            AbstractOutputStreamBuffer<> &inRelayStream)
        :   mRelayStream(inRelayStream), mError(false) { }

        /**
         * @brief If message is info/warning, relay output. Otherwise, hold it
         *        back.
         * 
         * arma::arma_stop throws a std::runtime_error with any empty what
         * string. The connector library is expected to test on runtime_errors
         * whether we have an error message available.
         */
        void output(char *inMsg, uint32_t inLength) {
            const char *kRunTimeErrStr = "run-time error: ";
            char *strStart = inMsg;
            char *strEnd;
            
            for (strStart = inMsg;
                std::isspace(*strStart, std::locale::classic());
                strStart++) ;
            if (std::strncmp(strStart, kRunTimeErrStr,
                    std::strlen(kRunTimeErrStr)) == 0) {
                
                for (strEnd = &inMsg[inLength - 1];
                    std::isspace(*strEnd, std::locale::classic());
                    strEnd--) ;
                *(++strEnd) = '\0';
                
                mTrimmedMsg = strStart;
                mError = true;
                return;
            } else if (*strStart == '\0') {
                // Ignore if the stream gets flushed with nothing in the buffer
                return;
            }
            
            mRelayStream.output(inMsg, inLength);
        }
        
        /**
         * @brief Return a C string containing the last error message, NULL if
         *        no error has occurred.
         *
         * @internal There is no need to reset the error message because an
         *           error will eventually become an exception.
         */
        const char *error() {
            if (mError)
                return mTrimmedMsg.c_str();
            
            return NULL;
        }
        
    protected:
        AbstractOutputStreamBuffer<> &mRelayStream;
        std::string mTrimmedMsg;
        bool mError;
    };
    
public:
    AbstractDBInterface(
        AbstractOutputStreamBuffer<> *inInfoBuffer,
        AbstractOutputStreamBuffer<> *inErrorBuffer)
    :   out(inInfoBuffer),
        err(inErrorBuffer),
        mArmadilloOutputStreamBuffer(*inErrorBuffer),
        mArmadilloOut(&mArmadilloOutputStreamBuffer) { }
    
    virtual ~AbstractDBInterface() { }
    
    /**
     * @brief Return the memory allocator for this DBMS backend
     * @param inMemContext Memory context the returned allocator shall use.
     *
     * @internal This function should be allowed to have side effects, so we do
     *     not declare it as const.
     */
    virtual AllocatorSPtr allocator(
        AbstractAllocator::Context inMemContext = AbstractAllocator::kFunction,
        AbstractAllocator::ZeroMemory inZeroMemory = AbstractAllocator::kDoNotZero)
        = 0;
    
    /**
     * @brief Return the last error message that did not cause an exception (if any).
     * 
     * Subclasses should call this methods as first step, and only if the return
     * value is NULL, they may report own last errors.
     *
     * The implementation in AbstractDBInterface only checks for errors
     * reported by Armadillo.
     *
     * @see ArmadilloOutputStreamBuffer
     */
    virtual const char *lastError() {
        return mArmadilloOutputStreamBuffer.error();
    }
    
    std::ostream out;
    std::ostream err;
    
protected:
    ArmadilloOutputStreamBuffer mArmadilloOutputStreamBuffer;
    std::ostream mArmadilloOut;
};
