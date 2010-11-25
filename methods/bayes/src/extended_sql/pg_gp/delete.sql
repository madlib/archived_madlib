-- Create a simple product aggregate. Trust on the optimizer for good performance.

DROP AGGREGATE madlib.product(DOUBLE PRECISION) CASCADE;

DROP FUNCTION madlib.product(DOUBLE PRECISION, DOUBLE PRECISION);

DROP AGGREGATE madlib.argmax(INTEGER, DOUBLE PRECISION);

DROP FUNCTION madlib.argmax_transition(
	oldmax madlib.ARGS_AND_VALUE_DOUBLE,
	newkey INTEGER,
	newvalue DOUBLE PRECISION);

DROP FUNCTION madlib.argmax_combine(
	max1 madlib.ARGS_AND_VALUE_DOUBLE, max2 madlib.ARGS_AND_VALUE_DOUBLE);

DROP FUNCTION madlib.argmax_final(finalstate madlib.ARGS_AND_VALUE_DOUBLE);

DROP FUNCTION madlib.bayes_training_sql(inTableName VARCHAR,
	inClassColumn VARCHAR,
	inAttrColumn VARCHAR,
	inNumAttrs INTEGER);

DROP FUNCTION madlib.bayes_training(
	inTableName VARCHAR,
	inClassColumn VARCHAR,
	inAttrColumn VARCHAR,
	inNumAttrs INTEGER);

DROP FUNCTION madlib.create_bayes_training_view(
	inTableName VARCHAR,
	inClassColumn VARCHAR,
	inAttrColumn VARCHAR,
	inNumAttrs INTEGER,
	inViewName VARCHAR);
	
DROP FUNCTION madlib.nb_classify(
	inTableName VARCHAR,
	inClassColumn VARCHAR,
	inAttrColumn VARCHAR,
	inNumAttrs INTEGER,
	inClassifyAttributes INTEGER[]);

DROP TABLE madlib.bayes;

DROP TABLE madlib.toclassify;

DROP TYPE madlib.ARGS_AND_VALUE_DOUBLE;