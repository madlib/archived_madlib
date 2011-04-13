/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractHandle.hpp
 *
 *//* ----------------------------------------------------------------------- */

class AbstractHandle {
public:
    virtual void *ptr() = 0;
    virtual MemHandleSPtr clone() const = 0;
};
