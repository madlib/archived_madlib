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
 * This class proved a convenient way to convert a size_list into a type which
 * is suitable for use with multi_array constructors.
template <std::size_t NumDims>
class shapeToExtents {
    typedef boost::multi_array_types::extent_gen::gen_type<NumDims> type;
    typedef boost::array< boost::multi_array_types::size_type , NumDims> SizeList;

public:
    shapeToExtents(const SizeList &inSizeList)
        : mExtents(
            internal_shapeToExtents<NumDims, NumDims - 1>::get(inSizeList))
        { }
    
    operator const type&() const {
        return mExtents;
    }
    
protected:
    const type mExtents;
}; */

template <std::size_t NumDims>
boost::detail::multi_array::extent_gen<NumDims>
shapeToExtents(const boost::multi_array_types::size_type *inSizeList) {
    return internal_shapeToExtents<NumDims, NumDims - 1>::get(inSizeList);
}



} // namespace modules

} // namespace regress

#endif
