DROP FUNCTION madlib.init_python_paths();

DROP FUNCTION madlib.update_nb_classification(
	"featureProbsSource" VARCHAR,
	"classPriorsSource" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"destColumn" VARCHAR);

DROP FUNCTION madlib.create_nb_classify_fn(
	"trainingSource" VARCHAR,
	"trainingClassColumn" VARCHAR,
	"trainingAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR);

DROP FUNCTION madlib.create_nb_classify_fn(
	"featureProbsSource" VARCHAR,
	"classPriorsSource" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR);

DROP FUNCTION madlib.create_nb_probs_view(
	"featureProbsSource" VARCHAR,
	"classPriorsSource" VARCHAR,
	"classifySource" VARCHAR,
	"classifyKeyColumn" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR);

DROP FUNCTION madlib.create_nb_probs_view(
	"trainingSource" VARCHAR,
	"trainingClassColumn" VARCHAR,
	"trainingAttrColumn" VARCHAR,
	"classifySource" VARCHAR,
	"classifyKeyColumn" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR);

DROP FUNCTION madlib.create_nb_classify_view(
	"trainingSource" VARCHAR,
	"trainingClassColumn" VARCHAR,
	"trainingAttrColumn" VARCHAR,
	"classifySource" VARCHAR,
	"classifyKeyColumn" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR);

DROP FUNCTION madlib.create_nb_classify_view(
	"featureProbsSource" VARCHAR,
	"classPriorsSource" VARCHAR,
	"classifySource" VARCHAR,
	"classifyKeyColumn" VARCHAR,
	"classifyAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"destName" VARCHAR);

DROP FUNCTION madlib.create_nb_prepared_data_tables(
	"trainingSource" VARCHAR,
	"trainingClassColumn" VARCHAR,
	"trainingAttrColumn" VARCHAR,
	"numAttrs" INTEGER,
	"featureProbsDestName" VARCHAR,
	"classPriorsDestName" VARCHAR);

DROP AGGREGATE madlib.argmax(INTEGER, DOUBLE PRECISION);

DROP FUNCTION madlib.argmax_final(finalstate madlib.ARGS_AND_VALUE_DOUBLE);

DROP FUNCTION madlib.argmax_combine(
	max1 madlib.ARGS_AND_VALUE_DOUBLE, max2 madlib.ARGS_AND_VALUE_DOUBLE);

DROP FUNCTION madlib.argmax_transition(
	oldmax madlib.ARGS_AND_VALUE_DOUBLE,
	newkey INTEGER,
	newvalue DOUBLE PRECISION);

DROP TYPE madlib.ARGS_AND_VALUE_DOUBLE;
