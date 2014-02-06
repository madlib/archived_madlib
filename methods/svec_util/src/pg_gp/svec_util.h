#ifndef SVECUTIL_H
#define SVECUTIL_H

Datum svec_l2norm(PG_FUNCTION_ARGS);
Datum svec_count(PG_FUNCTION_ARGS);
Datum svec_log(PG_FUNCTION_ARGS);
Datum svec_unnest(PG_FUNCTION_ARGS);
Datum svec_pivot(PG_FUNCTION_ARGS);
Datum svec_hash(PG_FUNCTION_ARGS);

Datum svec_svec_l1norm(PG_FUNCTION_ARGS);
Datum svec_svec_l2norm(PG_FUNCTION_ARGS);
Datum svec_svec_angle(PG_FUNCTION_ARGS);
Datum svec_svec_tanimoto_distance(PG_FUNCTION_ARGS);

#endif
