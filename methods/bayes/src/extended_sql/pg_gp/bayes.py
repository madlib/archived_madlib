import plpy



# We need to compute the following formula:
#                                              
#                                               __n
#   classify(a_1, ..., a_n) = argmax_c p(C = c) ||    p(A_i = a_i | C = c)
#                                                 i=1
#
# where probabilites are estimated with relative frquencies from the training
# set. See also: http://en.wikipedia.org/wiki/Naive_Bayes_classifier

# For the Naive Bayes Classification, we need a product over probabilities.
# However, multiplying lots of small numbers can lead to an exponent overflow.
# E.g., multiplying more than 324 numbers at most 0.1 will yield a product of 0
# in machine arithmetic. A safer way is therefore summing logarithms.
#
# By the IEEE 754 standard, the smallest number representable as
# DOUBLE PRECISION (64bit) is 2^(-1022), i.e., approximately 2.225e-308.
# See, e.g., http://en.wikipedia.org/wiki/Double_precision
# Hence, log(x) = log_10(x) for any non-zero DOUBLE PRECISION x is >= -308.
#
# Note for theorists:
# - Even adding infinitely many log_10(x) for 0 < x <= 1 will never cause
#   an overflow because addition will have no effect once the sum reaches approx
#   -308 * 2^53 (correspnding to the machine precision).

def get_feature_probs_sql(**kwargs):
	return """
	SELECT
		classes.class,
		triples.attr,
		triples.value,
		log(triples.cnt) - log(classes.class_cnt) AS log_feature_prob
	FROM
	(
		SELECT class, attr, value, count(*) AS cnt
		FROM
		(
			SELECT
				{trainingClassColumn} AS class,
				generate_series(1, {numAttrs}) AS attr,
				unnest( {trainingAttrColumn}[1: {numAttrs}] ) AS value
			FROM {trainingSource}
		) AS training_unnested
		GROUP BY class, attr, value
	) AS triples,
	{classPriorsSource} AS classes
	WHERE triples.class = classes.class
	""".format(**kwargs)


def get_class_priors_sql(**kwargs):
	return """
	SELECT
		{trainingClassColumn} AS class,
		count(*) AS class_cnt,
		sum(count(*)) OVER () AS all_cnt
	FROM {trainingSource}
	GROUP BY class
	""".format(**kwargs)


def get_keys_and_prob_values_sql(**kwargs):
	return """
	SELECT
		classify.key,
		classPriors.class,
		CASE WHEN count(*) < {numAttrs} THEN NULL
			 ELSE
				log(classPriors.class_cnt::DOUBLE PRECISION / classPriors.all_cnt)
				+ sum(featureProbs.log_feature_prob)
			 END
		AS log_prob
	FROM
		{featureProbsSource} AS featureProbs,
		{classPriorsSource} AS classPriors,
		(
			SELECT
				{classifyKeyColumn} AS key,
				generate_series(1, {numAttrs}) AS attr,
				unnest({classifyAttrColumn}) AS value
			FROM {classifySource}
		) AS classify
	WHERE
		featureProbs.class = classPriors.class AND
		featureProbs.attr = classify.attr AND featureProbs.value = classify.value
	GROUP BY classify.key, classPriors.class, classPriors.class_cnt, classPriors.all_cnt
	
	UNION
	
	SELECT
		classify.{classifyKeyColumn} AS key,
		classes.class,
		NULL
	FROM
		{classifySource} AS classify,
		{classPriorsSource} AS classes
	GROUP BY key, class
	""".format(**kwargs)


def get_prob_values_sql(**kwargs):
	return """
	SELECT
		classPriors.class,
		CASE WHEN count(*) < {numAttrs} THEN NULL
			 ELSE
				log(classPriors.class_cnt::DOUBLE PRECISION / classPriors.all_cnt)
				+ sum(featureProbs.log_feature_prob)
			 END
		AS log_prob
	FROM
		{featureProbsSource} AS featureProbs,
		{classPriorsSource} AS classPriors,
		(
			SELECT
				generate_series(1, {numAttrs}) AS attr,
				unnest({classifyAttrColumn}) AS value
		) AS classify
	WHERE
		featureProbs.class = classPriors.class AND
		featureProbs.attr = classify.attr AND featureProbs.value = classify.value
	GROUP BY classPriors.class, classPriors.class_cnt, classPriors.all_cnt
	
	UNION
	
	SELECT
		classes.class,
		NULL
	FROM
		{classPriorsSource} AS classes
	""".format(**kwargs)


def get_classification_sql(**kwargs):
	return """
		SELECT
			key,
			madlib.argmax(class, log_prob) AS nb_classification,
			max(log_prob) AS nb_log_probability
		FROM {keys_and_prob_values} AS keys_and_nb_values
		GROUP BY key
		""".format(
			keys_and_prob_values = "(" + get_keys_and_prob_values_sql(**kwargs) + ")"
		)


def create_prepared_data(**kwargs):
	kwargs.update(dict(
			sql = get_class_priors_sql(**kwargs)
		))
	plpy.execute("""
		CREATE {whatToCreate} {classPriorsDestName} AS
		{sql}
		""".format(**kwargs)
		)
	
	kwargs.update(dict(
			classPriorsSource = kwargs['classPriorsDestName']
		))
	kwargs.update(dict(
			sql = get_feature_probs_sql(**kwargs)
		))
	plpy.execute("""
		CREATE {whatToCreate} {featureProbsDestName} AS
		{sql}
		""".format(**kwargs)
		)


def create_classification(**kwargs):
	init_prepared_data(kwargs)
	kwargs.update(dict(
		keys_and_prob_values = "(" + get_keys_and_prob_values_sql(**kwargs) + ")"
		))
	plpy.execute("""
		CREATE {whatToCreate} {destName} AS
		SELECT
			key,
			madlib.argmax(class, log_prob) AS nb_classification,
			max(log_prob) AS nb_log_probability
		FROM {keys_and_prob_values} AS keys_and_nb_values
		GROUP BY key
		""".format(**kwargs))


def create_bayes_probabilities(**kwargs):
	init_prepared_data(kwargs)
	kwargs.update(dict(
		keys_and_prob_values = "(" + get_keys_and_prob_values_sql(**kwargs) + ")"
		))
	plpy.execute("""
		CREATE {whatToCreate} {destName} AS
		SELECT
			key,
			class,
			pow(10, max(log_prob)) /
				sum(pow(10, max(log_prob))) OVER (PARTITION BY key) AS nb_prob
		FROM
			{keys_and_prob_values} AS keys_and_nb_values
		GROUP BY
			key, class
		ORDER BY
			key, class
		""".format(**kwargs))


def create_classification_function(**kwargs):
	init_prepared_data(kwargs)
	kwargs.update(dict(
		classifyAttrColumn = "$1"
		))
	kwargs.update(dict(
		keys_and_prob_values = "(" + get_prob_values_sql(**kwargs) + ")"
		))
	plpy.execute("""
		CREATE FUNCTION {destName} (inAttributes INTEGER[])
		RETURNS INTEGER[] AS
		$$
			SELECT
				madlib.argmax(class, log_prob)
			FROM {keys_and_prob_values} AS key_and_nb_values
		$$
		LANGUAGE sql STABLE
		""".format(**kwargs))


def update_classification(**kwargs):
	init_prepared_data(kwargs)
	kwargs.update(dict(
		classifyAttrColumn = "destTable.{classifyAttrColumn}".format(**kwargs)
		))
	kwargs.update(dict(
		keys_and_prob_values = "(" + get_prob_values_sql(**kwargs) + ")"
		))
	plpy.execute("""
		UPDATE {destName} AS destTable
		SET {destColumn} = (
			SELECT
				madlib.argmax(class, log_prob)
			FROM {keys_and_prob_values} AS key_and_nb_values
		)
		""".format(**kwargs))


def init_prepared_data(kwargs):
	if not 'classPriorsSource' in kwargs:
		kwargs.update(dict(
				classPriorsSource = "(" + get_class_priors_sql(**kwargs) + ")" 
			))
	if not 'featureProbsSource' in kwargs:
		kwargs.update(dict(
				featureProbsSource = "(" + get_feature_probs_sql(**kwargs) + ")"
			))
