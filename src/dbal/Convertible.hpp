/* ----------------------------------------------------------------------- *//**
 *
 * @file Convertible.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Tagging interface for classes that  Interface for classes that provide a conversion function to \c T
 */
template <class T>
class Convertible {
public:
    virtual operator T() const = 0;
};
