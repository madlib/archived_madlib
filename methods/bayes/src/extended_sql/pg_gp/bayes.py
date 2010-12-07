import plpy



# We need to compute the following formula:
#                                              
#                                               __n
#   classify(a_1, ..., a_n) = argmax_c p(C = c) ||    p(A_i = a_i | C = c)
#                                                 i=1
#
# where probabilites are estimated with relative frquencies from the training
# set. See also: http://en.wikipedia.org/wiki/Naive_Bayes_classifier
#
# There are different ways to estimate the feature probabilities
# p(A_i = a_i | C = c). The maximum likelyhood exstimate takes the relative
# frequencies. That is:
#
#                        #(i,f,c)
#   P(A_i = a | C = c) = --------
#                           #c
#
# where:
# #(i,f,c) = # of training samples where attribute i is f and class is c
# #c       = # of training samples where class is c
#
# Since the maximum likelyhood sometimes results in estimates of 0, it is
# often desirable to use a "smoothed" estimate. Intuitively, one adds a number
# of "virtual" samples and assumes that these samples are evenly distributed
# among the values attribute i can assume (i.e., the set of all values observed
# for attribute a for any class):
#
#                        #(i,f,c) + s
#   P(A_i = a | C = c) = ------------
#                         #c + s * #i
#
# where:
# #i       = # of distinct values for attribute i (for all classes)
# s        = smoothing factor (>= 0).
#
# The case s = 1 is known as "Laplace smoothing" in the literature. The case
# s = 0 trivially reduces to maximum likelyhood estimates.
#
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
	if not 'attrValuesSource' in kwargs:
		kwargs.update(dict(
				attrValuesSource = "(" + get_attr_values_sql(**kwargs) + ")"
			))
	if not 'attrCountsSource' in kwargs:
		kwargs.update(dict(
				attrCountsSource = "(" + get_attr_counts_sql(**kwargs) + ")"
			))

	return """
	SELECT
		class,
		attr,
		value,
		coalesce(cnt, 0) AS cnt,
		attr_cnt
	FROM
	(
		SELECT *
		FROM
			{classPriorsSource} AS classes
		CROSS JOIN
			{attrValuesSource} AS attr_values
	) AS required_triples
	LEFT OUTER JOIN
	(
		SELECT
			{trainingClassColumn} AS class,
			attr,
			{trainingAttrColumn}[attr] AS value,
			count(*) AS cnt
		FROM
			generate_series(1, {numAttrs}) AS attr,
			{trainingSource}
		GROUP BY class, attr, value
	) AS triple_counts
	USING (class, attr, value)
	INNER JOIN
		{attrCountsSource} AS attr_counts
	USING (attr)
	""".format(**kwargs)


def get_attr_values_sql(**kwargs):
	"""SQL string for listing all (attribute, value) pairs in the training data.

	Note: If PostgreSQL supported count(DISTINCT ...) for window functions, we could
	consolidate this function with get_attr_counts_sql():
	[...] count(DISTINCT value) OVER (PARTITION BY attr) [...]
	
	"""
	return """
	SELECT DISTINCT
		attr,
		{trainingAttrColumn}[attr] AS value
	FROM
		generate_series(1, {numAttrs}) AS attr,
		{trainingSource}
	""".format(**kwargs)


def get_attr_counts_sql(**kwargs):
	return """
	SELECT
		attr,
		count(DISTINCT {trainingAttrColumn}[attr]) AS attr_cnt
	FROM
		generate_series(1, {numAttrs}) AS attr,
		{trainingSource}
	GROUP BY attr
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
				+ sum( log((featureProbs.cnt::DOUBLE PRECISION + 1) / (classPriors.class_cnt + featureProbs.attr_cnt)) )
			 END
		AS log_prob
	FROM
		{featureProbsSource} AS featureProbs,
		{classPriorsSource} AS classPriors,
		(
			SELECT
				{classifyKeyColumn} AS key,
				attr,
				{classifyAttrColumn}[attr] AS value
			FROM
				{classifySource},
				generate_series(1, {numAttrs}) AS attr
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
	if not 'smoothingFactor' in kwargs:
		kwargs.update(dict(
			smoothingFactor = 1
		))
	
	return """
	SELECT
		classPriors.class,
		CASE WHEN count(*) < {numAttrs} THEN NULL
			 ELSE
				log(classPriors.class_cnt::DOUBLE PRECISION / classPriors.all_cnt)
				+ sum( log((featureProbs.cnt::DOUBLE PRECISION + {smoothingFactor})
					/ (classPriors.class_cnt + {smoothingFactor} * featureProbs.attr_cnt)) )
			 END
		AS log_prob
	FROM
		{featureProbsSource} AS featureProbs,
		{classPriorsSource} AS classPriors,
		(
			SELECT
				attr,
				{classifyAttrColumn}[attr] AS value
			FROM
				generate_series(1, {numAttrs}) AS attr
		) AS classify
	WHERE
		featureProbs.class = classPriors.class AND
		featureProbs.attr = classify.attr AND featureProbs.value = classify.value AND
		({smoothingFactor} > 0 OR featureProbs.cnt > 0)
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
	"""Precompute all class priors and feature probabilities.
	
	When the precomputations are to be stored in a table, we add indices and
	let ANALYZE run.
	FIXME: This is not portable.
	"""

	if kwargs['whatToCreate'] == 'TABLE':
		plpy.execute("""
			CREATE TEMPORARY TABLE tmp_attr_counts
			AS
			{attr_counts_sql};
			ALTER TABLE tmp_attr_counts ADD PRIMARY KEY (attr);
			ANALYZE tmp_attr_counts;
			
			CREATE TEMPORARY TABLE tmp_attr_values
			AS
			{attr_values_sql};
			ALTER TABLE tmp_attr_values ADD PRIMARY KEY (attr, value);
			ANALYZE tmp_attr_values;
			""".format(
				attr_counts_sql = "(" + get_attr_counts_sql(**kwargs) + ")",
				attr_values_sql = "(" + get_attr_values_sql(**kwargs) + ")"
				)
			)
		kwargs.update(dict(
			attrValuesSource = 'tmp_attr_values',
			attrCountsSource = 'tmp_attr_counts'
		))


	kwargs.update(dict(
			sql = get_class_priors_sql(**kwargs)
		))
	plpy.execute("""
		CREATE {whatToCreate} {classPriorsDestName}
		AS
		{sql}
		""".format(**kwargs)
		)
	if kwargs['whatToCreate'] == 'TABLE':
		plpy.execute("""
			ALTER TABLE {classPriorsDestName} ADD PRIMARY KEY (class);
			ANALYZE {classPriorsDestName};
			""".format(**kwargs))
	
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
	if kwargs['whatToCreate'] == 'TABLE':
		plpy.execute("""
			ALTER TABLE {featureProbsDestName} ADD PRIMARY KEY (class, attr, value);
			ANALYZE {featureProbsDestName};
			DROP TABLE tmp_attr_counts;
			DROP TABLE tmp_attr_values;
			""".format(**kwargs))


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
	"""Create table/view that, for each row to be classified, contains probabilities for each class.
	
	FIXME: The expression pow(10, max(log_prob)) can cause an underflow in which
	case PostgreSQL will unfortunately raise en error."""

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
		classifyAttrColumn = "$1",
		smoothingFactor = "$2"
		))
	kwargs.update(dict(
		keys_and_prob_values = "(" + get_prob_values_sql(**kwargs) + ")"
		))
	plpy.execute("""
		CREATE FUNCTION {destName} (inAttributes INTEGER[], inSmoothingFactor DOUBLE PRECISION)
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
