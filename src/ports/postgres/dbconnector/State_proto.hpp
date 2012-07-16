/* ----------------------------------------------------------------------- *//**
 *
 * @file State_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_STATE_PROTO_HPP
#define MADLIB_POSTGRES_STATE_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

template <typename T>
class Ref {
public:
    Ref() : mPtr(NULL) { }

    Ref(T* inPtr) : mPtr(inPtr) { }

    Ref& rebind(T* inPtr) {
        mPtr = inPtr;
        return *this;
    }

    Ref& operator=(const T& inValue) {
        *mPtr = inValue;
        return *this;
    }

    operator const T&() const {
        return *mPtr;
    }

    operator T&() {
        return *mPtr;
    }

    const T* ptr() const {
        return mPtr;
    }

    bool isNull() const {
        return mPtr == NULL;
    }

protected:
    T *mPtr;
};

template <class Storage, bool IsMutable = Storage::isMutable>
struct StorageTraits;

template <class StreamBuf>
class BinaryStream {
public:
    typedef StreamBuf StreamBuf_type;
    typedef typename StreamBuf::Traits_type Traits_type;
    typedef typename Traits_type::char_type char_type;
    enum { isMutable = Traits_type::isMutable };

    friend class DryMode;

    class DryMode {
    public:
        DryMode(BinaryStream& inStream) : mStream(inStream), mIsIn(true) {
            mStream.enterDryMode();
        }

        ~DryMode() {
            leave();
        }

        void leave() {
            if (mIsIn) {
                mStream.leaveDryMode();
                mIsIn = false;
            }
        }

    protected:
        BinaryStream& mStream;
        bool mIsIn;
    };

    BinaryStream(StreamBuf_type* inStreamBuf)
      : mStreamBuf(inStreamBuf), mDryMode(false) { }

    template <size_t Alignment> size_t seek(std::ptrdiff_t inOffset,
        std::ios_base::seekdir inDir);

    size_t seek(std::ptrdiff_t inOffset, std::ios_base::seekdir inDir);

    size_t available() const;

    size_t seek(size_t inPos) {
        return mStreamBuf->seek(inPos);
    }

    char_type* ptr() {
        return mStreamBuf->ptr();
    }

    size_t size() const {
        return mStreamBuf->size();
    }

    size_t tell() const;

    std::ios_base::iostate rdstate() const;

    bool eof() const;

    bool isInDryMode() const {
        return mDryMode > 0;
    }

    template <class T> T* read(size_t inCount = 1);

protected:
    void enterDryMode() {
        ++mDryMode;
    }

    void leaveDryMode() {
        madlib_assert(mDryMode > 0, std::logic_error("Non-positive dry-mode "
            "counter detected."));
        --mDryMode;
    }

    /**
     * @brief The associated storage to the stream (similar to a streambuf for
     *     IOStreams)
     */
    StreamBuf_type* mStreamBuf;

    /**
     * @brief In dry mode read/write operations will only move the cursor, but
     *     other objects are not touched
     */
    int32_t mDryMode;
};


template <class Storage, class Traits = StorageTraits<Storage>,
    bool IsMutable = Traits::isMutable>
class HandleStreamBuf {
public:
    typedef Storage Storage_type;
    typedef Traits Traits_type;
    typedef typename Traits_type::char_type char_type;
    enum { isMutable = Traits_type::isMutable };

    HandleStreamBuf(const Storage_type& inStorage)
      : mStorage(inStorage), mPos(0) { }

    // Binary Stream concept

    size_t seek(size_t inPos) {
        mPos = inPos;

        if (inPos > size())
            return size_t(-1);
        else
            return mPos;
    }

    char_type* ptr() {
        return storage().ptr();
    }

    size_t size() const {
        return storage().size();
    }

    size_t tell() const {
        return mPos;
    }

    void setStorage(Storage_type& inStorage) {
        mStorage = inStorage;
    }

    Storage_type storage() {
        return const_cast<Storage_type&>(
            static_cast<const HandleStreamBuf*>(this)->storage()
        );
    }

    const Storage_type& storage() const {
        return mStorage;
    }

    void resize(size_t inSize, size_t inPivot) {
        if (inSize == this->size())
            return;

        char_type* oldPtr = this->ptr();
        size_t secondChunkStart
            = inPivot > this->size() ? this->size() : inPivot;
        size_t oldSize = this->size();
        ptrdiff_t delta = inSize - oldSize;

        // FIXME: Deallocate old memory!
        this->mStorage = defaultAllocator().allocateByteString<
            dbal::FunctionContext, dbal::DoZero, dbal::ThrowBadAlloc>(inSize);

        std::copy(oldPtr, oldPtr + secondChunkStart, this->ptr());
        std::copy(oldPtr + secondChunkStart, oldPtr + oldSize,
            this->ptr() + secondChunkStart + delta);
        std::fill(this->ptr() + secondChunkStart,
            this->ptr() + secondChunkStart + delta, 0);
    }

protected:
    Storage mStorage;
    size_t mPos;
};

template <class Storage>
class RootState : public HandleStreamBuf<Storage> {
public:
    typedef HandleStreamBuf<Storage> Base;
    typedef typename Base::Storage_type Storage_type;

    RootState(const Storage_type& inStorage)
      : Base(inStorage) { }
};


template <class Derived, class Parent, bool IsMutable = Parent::isMutable>
class StateBase {
public:
    typedef Parent Parent_type;
    typedef typename Parent_type::RootState_type RootState_type;
    typedef typename Parent_type::Storage_type Storage_type;
    typedef typename Parent_type::BinaryStream_type BinaryStream_type;
    typedef Parent_type Init_type;
    typedef typename Parent_type::Traits_type Traits_type;
    enum { isMutable = Parent_type::isMutable };

    StateBase(Init_type& inParent);
    void initialize();
    const RootState_type& rootState() const;
//    const Storage_type& storage() const;
//    const BinaryStream_type& binaryStream() const;

protected:
    Parent_type& mParent;
};

template <class Derived, class Parent>
class StateBase<Derived, Parent, true /* IsMutable */>
  : public StateBase<Derived, Parent, false> {
public:
    typedef StateBase<Derived, Parent, false> Base;
    typedef typename Base::Init_type Init_type;

    StateBase(Init_type& inParent);

protected:
    template <class SubState> void setSize(SubState &inSubState, size_t inSize);

    using Base::mParent;
};

template <class Derived, bool IsMutable, class Storage>
class StateBase<Derived, RootState<Storage>, IsMutable> {
public:
    typedef RootState<Storage> Parent_type;
    typedef Derived RootState_type;
    typedef typename Parent_type::Storage_type Storage_type;
    typedef BinaryStream<Parent_type> BinaryStream_type;
    typedef const Storage_type Init_type;
    typedef typename Parent_type::Traits_type Traits_type;

    StateBase(Init_type& inStorage);
    void initialize();
    const RootState_type& rootState() const;
    const Storage_type& storage() const;
    const BinaryStream_type& binaryStream() const;

protected:
    Parent_type mParent;
    BinaryStream_type mBinaryStream;
};

template <class Derived, class Storage>
class StateBase<Derived, RootState<Storage>, true /* IsMutable */>
  : public StateBase<Derived, RootState<Storage>, false> {

public:
    typedef StateBase<Derived, RootState<Storage>, false> Base;
    typedef typename Base::Init_type Init_type;

    StateBase(Init_type& inStorage) : Base(inStorage) { }
    void initialize();

protected:
    template <class SubState> void setSize(SubState &inSubState, size_t inSize);

    using Base::mParent;
    using Base::mBinaryStream;
};


template <class Derived, class Parent, bool IsMutable = Parent::isMutable>
class State : public StateBase<Derived, Parent, Parent::isMutable> {
public:
    typedef StateBase<Derived, Parent> Base;
    typedef typename Base::Storage_type Storage_type;
    typedef typename Base::RootState_type RootState_type;
    typedef typename Base::BinaryStream_type BinaryStream_type;
    typedef typename Base::Init_type Init_type;
    typedef typename Base::Traits_type Traits_type;
    typedef typename Traits_type::char_type char_type;

    State(Init_type& inInitialization);

    using Base::rootState;
    RootState_type& rootState();
    using Base::storage;
    Storage_type& storage();
    using Base::binaryStream;
    BinaryStream_type& binaryStream();

    size_t begin() const;
    size_t end() const;
    const char_type* ptr() const;
    char_type* ptr();
    size_t size() const;


    /**
     * @brief Bind a State object to the current position in the stream
     *
     * The following idiom (in-class friend function together with static
     * polymorphicsm) is called the "Barton-Nackman trick". Essentially, we only
     * make operator>> visible if type Derived is involved.
     *
     * Subclasses have to implement the bind() function.
     */
    friend
    BinaryStream_type&
    operator>>(BinaryStream_type& inStream, Derived& inState) {
        inStream.template seek<MAXIMUM_ALIGNOF>(0, std::ios_base::cur);
        if (!inStream.isInDryMode())
            inState.mBegin = inStream.tell();
        inState.bind(inStream);
        inStream.template seek<MAXIMUM_ALIGNOF>(0, std::ios_base::cur);
        if (!inStream.isInDryMode())
            inState.mEnd = inStream.tell();
        return inStream;
    }

protected:
    size_t mBegin;
    size_t mEnd;
};

template <class Derived, class Parent>
class State<Derived, Parent, true /* IsMutable */>
  : public State<Derived, Parent, false> {

public:
    typedef State<Derived, Parent, false> Base;
    typedef typename Base::Init_type Init_type;
    typedef typename Base::BinaryStream_type BinaryStream_type;

    State(Init_type& inInitialization) : Base(inInitialization) { };

    using Base::setSize;
    void setSize(size_t inSize);
    void resize();

protected:
    template <class OtherDerived> State& copy(
        const State<OtherDerived, typename OtherDerived::Parent_type>&
            inOtherState);
};


#define MADLIB_STATE_TYPEDEFS(_Class, _Parent) \
    typedef State<_Class, _Parent> Base; \
    typedef typename Base::Init_type Init_type; \
    typedef typename Base::BinaryStream_type BinaryStream_type; \
    typedef typename Base::Storage_type Storage_type; \
    typedef typename Base::Traits_type Traits_type; \
    typedef typename Traits_type::double_type double_type; \
    typedef typename Traits_type::uint64_type uint64_type; \
    typedef typename Traits_type::uint32_type uint32_type; \
    typedef typename Traits_type::uint16_type uint16_type; \
    typedef typename Traits_type::MappedColumnVector_type MappedColumnVector_type; \
    typedef typename Traits_type::MappedMatrix_type MappedMatrix_type;


} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_STATE_PROTO_HPP)
