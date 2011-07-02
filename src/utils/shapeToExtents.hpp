/* ----------------------------------------------------------------------- *//**
 *
 * @file shapeToExtents.hpp
 *
 * @brief Integration classes for multi_array classes
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_SHAPETOEXTENTS_HPP
#define MADLIB_SHAPETOEXTENTS_HPP

namespace madlib {

namespace utils {

/**
 * @internal
 * @brief Helper struct for converting a size_type (an array of sizes) to Boost
 *        extents
 * 
 * There is no need to use this struct directly. User code should call
 * shapeToExtents(const boost::multi_array_types::size_type *inSizeList)
 * for conversion.
 */
template <std::size_t NumDims, std::size_t Dim>
struct internal_shapeToExtents {
    typedef boost::detail::multi_array::extent_gen<Dim + 1> type;
    typedef boost::multi_array_types::size_type size_type;

    static inline type get(const size_type *inSizeList) {
        return internal_shapeToExtents<NumDims, Dim - 1>::get(inSizeList).operator[](
            inSizeList[Dim] );
    }
};

template <std::size_t NumDims>
struct internal_shapeToExtents<NumDims, 0> {
    typedef boost::detail::multi_array::extent_gen<1> type;
    typedef boost::multi_array_types::size_type size_type;

    static inline type get(const size_type *inSizeList) {        
        return boost::extents[ inSizeList[0] ];
    }
};


/**
 * @brief convert a size_type (an array of sizes) to Boost extents
 */
template <std::size_t NumDims>
boost::detail::multi_array::extent_gen<NumDims>
shapeToExtents(const boost::multi_array_types::size_type *inSizeList) {
    return internal_shapeToExtents<NumDims, NumDims - 1>::get(inSizeList);
}



} // namespace modules

} // namespace regress

#endif
