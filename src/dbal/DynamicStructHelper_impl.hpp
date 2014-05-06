/* ----------------------------------------------------------------------- *//**
 *
 * @file DynamicStructHelper_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_DYNAMICSTRUCT_HELPER_IMPL_HPP
#define MADLIB_DBAL_DYNAMICSTRUCT_HELPER_IMPL_HPP

namespace madlib { namespace dbal {

// ------------------------------------------------------------------------
template <class Container>
inline DynamicStructHelper<Container>::DynamicStructHelper (Init_type& inInitialization) :
    Base(inInitialization)
{
    this->initialize();
}

// ------------------------------------------------------------------------

template <class Container>
template <class OtherContainer>
inline DynamicStructHelper<Container>& DynamicStructHelper<Container>::operator= (
    const DynamicStructHelper<OtherContainer>& inOther)
{
    this->copy(inOther);
    return *this;
}

} } // namespace madlib::dbal

#endif // defined(MADLIB_DBAL_DYNAMICSTRUCT_HELPER_IMPL_HPP)
