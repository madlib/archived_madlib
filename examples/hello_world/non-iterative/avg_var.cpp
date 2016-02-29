/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/* ----------------------------------------------------------------------- *//**
 *
 * @file avg_var.cpp
 *
 * @brief average population variance functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include "avg_var.hpp"

namespace madlib {

namespace modules {

namespace hello_world {

template <class Handle>
class AvgVarTransitionState {
    template <class OtherHandle>
    friend class AvgVarTransitionState;

public:
    AvgVarTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind();
    }

    /**
     * @brief Convert to backend representation
     *
     * We define this function so that we can use State in the
     * argument list and as a return type.
     */
    inline operator AnyType() const {
        return mStorage;
    }

    /**
     * @brief Update state with a new data point
     */
    AvgVarTransitionState & operator+=(const double x){
        double diff = (x - avg);
        double normalizer = static_cast<double>(numRows + 1);
        // online update mean
        this->avg += diff / normalizer;

        // online update variance
        double new_diff = (x - avg);
        double a = static_cast<double>(this->numRows) / normalizer;
        this->var = (var * a) + (diff * new_diff) / normalizer;
    return *this;
    }

    /**
     * @brief Merge with another state object
     *
     * We update mean and variance in a online fashion
     * to avoid intermediate large sum.
     */
    template <class OtherHandle>
    AvgVarTransitionState &operator+=(
        const AvgVarTransitionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size())
            throw std::logic_error("Internal error: Incompatible transition "
                                   "states");
        double avg_ = inOtherState.avg;
        double var_ = inOtherState.var;
        uint16_t numRows_ = static_cast<uint16_t>(inOtherState.numRows);
        double totalNumRows = static_cast<double>(numRows + numRows_);

        // we perform a weighted average between states
        double w = static_cast<double>(numRows) / totalNumRows;
        double w_ = static_cast<double>(numRows_) / totalNumRows;
        double totalAvg = avg * w + avg_ * w_;
        double a = avg - totalAvg;
        double a_ = avg_ - totalAvg;

        numRows += numRows_;
        this->var = (w * var) + (w_ * var_) + (w * a * a) + (w_ * a_ * a_);
        this->avg = totalAvg;
        return *this;
    }

  private:
    void rebind() {
        avg.rebind(&mStorage[0]);
        var.rebind(&mStorage[1]);
        numRows.rebind(&mStorage[2]);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToDouble avg;
    typename HandleTraits<Handle>::ReferenceToDouble var;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
};


AnyType
avg_var_transition::run(AnyType& args) {
    // get current state value
    AvgVarTransitionState<MutableArrayHandle<double> > state = args[0];
    // update state with current row value
    double x = args[1].getAs<double>();
    state += x;
    state.numRows ++;
    return state;
}

AnyType
avg_var_merge_states::run(AnyType& args) {
    AvgVarTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    AvgVarTransitionState<ArrayHandle<double> > stateRight = args[1];

    // Merge states together and return
    stateLeft += stateRight;
    return stateLeft;
}

AnyType
avg_var_final::run(AnyType& args) {
    AvgVarTransitionState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.numRows == 0)
        return Null();

    return state;
}
// -----------------------------------------------------------------------

} // namespace hello_world
} // namespace modules
} // namespace madlib
