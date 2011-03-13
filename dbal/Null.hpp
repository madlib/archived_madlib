/* ----------------------------------------------------------------------- *//**
 *
 * @file Null.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_NULL_HPP
#define MADLIB_NULL_HPP

#include <madlib/dbal/dbal.hpp>
#include <madlib/dbal/AbstractValue.hpp>

#include <stdexcept>



namespace madlib {

namespace dbal {

class Null : public AbstractValue {
public:
    bool isNull() const {
        return true;
    }
    
    static const Null &value() {
        return nullValue;
    }
    
    static const AbstractValueSPtr &sptr() {
        return nullPtr;
    }

protected:
    /**
     * Null is a singleton class. Hence, clone() just returns the only instance
     * there is.
     */
    AbstractValueSPtr clone() const {
        if (this != &Null::nullValue || &Null::nullValue != Null::nullPtr.get())
            throw std::logic_error("More than one null value");
    
        return Null::nullPtr;
    }

private:
    Null() {
    }

    static const Null nullValue;
    static const AbstractValueSPtr nullPtr;
};

} // namespace dbal

} // namespace madlib

#endif

