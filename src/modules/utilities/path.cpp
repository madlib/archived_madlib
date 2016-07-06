/* ----------------------------------------------------------------------- *//**
 * @file path.cpp
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>

#include <math.h>
#include <iostream>
#include <algorithm>
#include <functional>
#include <numeric>
#include <vector>
#include <string>
#include <boost/xpressive/xpressive.hpp>
#include "path.hpp"

namespace madlib {
namespace modules {
namespace utilities {

using namespace dbal::eigen_integration;
using namespace boost::xpressive;

AnyType path_pattern_match::run(AnyType & args)
{
    std::string sym_str = args[0].getAs<char *>();
    std::string reg_str = args[1].getAs<char *>();
    MappedColumnVector row_id = args[2].getAs<MappedColumnVector>();

    if (sym_str.size() != row_id.size()) {
        std::stringstream errorMsg;
        errorMsg << "dimensions mismatch: " << sym_str.size() <<
                    " != " << row_id.size() <<
                    "; #symbols must be equal to #rows!";
        throw std::invalid_argument(errorMsg.str());
    }

    // store the returned results
    std::vector<double> _match_id, _match_row_id;
    // call factory method to create dynamic regex object from a string
    sregex reg = sregex::compile(reg_str);
    const double* row_start = row_id.memoryHandle().ptr();
    std::string::const_iterator sym_start = sym_str.begin(),
                                start = sym_str.begin(),
                                end = sym_str.end();
    // also the id for the current match
    double match_count = 1.0;
    // reuse the match_results<> object
    smatch matches;
    // prefer regex_search over sregex_iterator so that match_results<> object
    // caches dynamically allocated memory across regex searches
    while (regex_search(start, end, matches, reg)) {
        size_t i0 = matches[0].first - sym_start;
        size_t i1 = matches[0].second - sym_start;
        _match_row_id.insert(_match_row_id.end(), row_start+i0, row_start+i1);
        _match_id.insert(_match_id.end(), matches[0].length(), match_count++);
        start = matches[0].second;
    }

    MappedColumnVector match_id(_match_id.data(), _match_id.size());
    MappedColumnVector match_row_id(_match_row_id.data(), _match_row_id.size());

    AnyType tuple;
    tuple << match_id << match_row_id;
    return tuple;
}

} // namespace utilities
} // namespace modules
} // namespace madlib
