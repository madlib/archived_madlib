/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractDBInterface.hpp
 *
 *//* ----------------------------------------------------------------------- */

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
            bool isRunTimeError = false;
            const char *kRunTimeErrStr = "run-time error: ";
            char *strStart = inMsg;
            char *strEnd;
            
            for (strStart = inMsg;
                std::isspace(*strStart, std::locale::classic());
                strStart++) ;
            if (std::strncmp(strStart, kRunTimeErrStr,
                    std::strlen(kRunTimeErrStr)) == 0) {
                
                isRunTimeError = true;
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
    
    // This function should be allowed to have side effects, so we do not
    // declare it as const.
    virtual AllocatorSPtr allocator(
        AbstractAllocator::Context inMemContext = AbstractAllocator::kFunction)
        = 0;
    
    /**
     * @brief Return the last error message that did not cause an exception (if any).
     * 
     * Subclasses should call this methods as first step, and only if the return
     * value is NULL, they may report own last errors.
     *
     * The implementation of this function in AbstractDBInterface is only for
     * Armadillo.
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
