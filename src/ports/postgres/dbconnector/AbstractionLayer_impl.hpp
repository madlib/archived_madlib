/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractionLayer_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

/**
 * @brief Return an AnyType object representing Null.
 *
 * @internal
 *     An object representing Null is not guaranteed to be unique. In fact, here
 *     we simply return an AnyType object initialized by the default
 *     constructor.
 *
 * @see AbstractionLayer::AnyType::AnyType()
 */
inline
AnyType
AbstractionLayer::Null() {
    return AnyType();
}

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
