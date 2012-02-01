#ifndef SPARSEVECTOR_OPERATORS_H
#define SPARSEVECTOR_OPERATORS_H

Datum svec_svec_l1norm(PG_FUNCTION_ARGS);
Datum svec_svec_l2norm(PG_FUNCTION_ARGS);
Datum svec_svec_angle(PG_FUNCTION_ARGS);
Datum svec_svec_tanimoto_distance(PG_FUNCTION_ARGS);

#endif
