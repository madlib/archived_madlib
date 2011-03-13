/* ----------------------------------------------------------------------- *//**
 *
 * @file Null.cpp
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/dbal/Null.hpp>
#include <madlib/utils/memory.hpp>

namespace madlib {

namespace dbal {

using utils::memory::NoDeleter;


const Null Null::nullValue;

const AbstractValueSPtr Null::nullPtr(
        &Null::nullValue, NoDeleter<const AbstractValue>());

} // namespace dbal

} // namespace madlib
