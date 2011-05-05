/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractHandle.hpp
 *
 *//* ----------------------------------------------------------------------- */

class AbstractHandle {
public:
    virtual ~AbstractHandle() { }
    virtual void *ptr() = 0;
    virtual MemHandleSPtr clone() const = 0;
};
