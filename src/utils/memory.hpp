/* ----------------------------------------------------------------------- *//**
 *
 * @file memory.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MEMORY_HPP
#define MADLIB_MEMORY_HPP

namespace madlib {

namespace utils {

namespace memory {

template<typename T>
struct ArrayDeleter {
    void operator()(T *p) {
        delete [] p;
    }
};

template <class T>
struct NoDeleter {
    void operator()(T * /* p */) { }
};

} // namespace prob

} // namespace modules

} // namespace regress

#endif
