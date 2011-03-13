#include <madlib/modules/regress/regression.hpp>


namespace madlib {

namespace modules {

namespace regress {

using dbal::ConcreteRecord;
using dbal::AnyValueVector;
using dbal::Null;

class MyTuple {
public:
    MyTuple(AnyValue::iterator element) {
        val1 =    element ->isNull() ? 0 : int32_t(*element);
        val2 = (++element)->isNull() ? 0 : int32_t(*element);
    }
    
    operator ConcreteRecord() {
        AnyValueVector valueVector;
        ConcreteRecord::iterator element(valueVector);
        
        element++ = val1;
        element++ = val2;
        element++ = Null::value();
        return valueVector;
    }

    int32_t     val1;
    int32_t     val2;
};

AnyValue multiply::operator()(AnyValue::iterator arg) const {
//    MyTuple tuple = arg;
    double val1 =  (++arg)->isNull() ? 1.1 : double(*arg);
    double val2 = *(++arg);
    
    AnyValueVector  returnValue;
    ConcreteRecord::iterator element(returnValue);

    element++ = val1 * val2;
    element++ = Null::value();
    return returnValue;        
}

AnyValue add::operator()(AnyValue::iterator arg) const {
    double val1 = *arg, val2 = *(++arg);
    
    return val1 + val2;
}

AnyValue subtract::operator()(AnyValue::iterator arg) const {
    double val1 = *arg, val2 = *(++arg);
    
    return val1 - val2;
}

} // namespace regress

} // namespace modules

} // namespace madlib
