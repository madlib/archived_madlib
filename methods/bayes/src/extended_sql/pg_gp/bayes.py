import plpy

# A Naive Bayes classifier computes the following formula:
#                                              
#                                               __n
#   classify(a_1, ..., a_n) = argmax_c P(C = c) ||    P(A_i = a_i | C = c)
#                                                 i=1
#
# where probabilites are estimated with relative frequencies from the training
# set. See also: http://en.wikipedia.org/wiki/Naive_Bayes_classifier
#
# There are different ways to estimate the feature probabilities
# p(A_i = a_i | C = c). The maximum likelihood exstimate takes the relative
# frequencies. That is:
#
#                        #(c,i,a)
#   P(A_a = v | C = c) = --------
#                           #c
#
# where:
# #(i,a,c) = # of training samples where attribute i is a and class is c
# #c       = # of training samples where class is c
#
# Since the maximum likelihood sometimes results in estimates of 0, it might
# be desirable to use a "smoothed" estimate. Intuitively, one adds a number of
# "virtual" samples and assumes that these samples are evenly distributed
# among the values attribute i can assume (i.e., the set of all values observed
# for attribute a for any class):
#
#                        #(c,i,a) + s
#   P(A_i = a | C = c) = ------------
#                         #c + s * #i
#
# where:
# #i       = # of distinct values for attribute i (for all classes)
# s        = smoothing factor (>= 0).
#
# The case s = 1 is known as "Laplace smoothing" in the literature. The case
# s = 0 trivially reduces to maximum likelihood estimates.
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



# Return query mapping triples (class c, attribute i, value a) to #(c,i,a) and
# #a. For each class and for each pair (attribute, value) occuring in the
# training data (for any class), the query will contain a row (so it contains
# rows with where #(c,i,a) = 0).

def get_feature_probs_sql(**kwargs):
	"""Return query mapping triples (class, attribute, value) to number of
	occurrences in training data and number of distinct values for attribute."""

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


# Return query listing all pairs (attribute, value) in the training data.
#
# Note: If PostgreSQL supported count(DISTINCT ...) for window functions, we could
# consolidate this function with get_attr_counts_sql():
# [...] count(DISTINCT value) OVER (PARTITION BY attr) [...]

def get_attr_values_sql(**kwargs):
	return """
		SELECT DISTINCT
			attr,
			{trainingAttrColumn}[attr] AS value
		FROM
			generate_series(1, {numAttrs}) AS attr,
			{trainingSource}
		""".format(**kwargs)


# Return query mapping all attributes to the number of distinct values each
# attribute assumes in the training data.

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


# Return query mapping all classes c to the number of records in the training
# data classified as c. Moreover, the total number of record in the training
# data is returned.

def get_class_priors_sql(**kwargs):
	return """
		SELECT
			{trainingClassColumn} AS class,
			count(*) AS class_cnt,
			sum(count(*)) OVER () AS all_cnt
		FROM {trainingSource}
		GROUP BY class
		""".format(**kwargs)


# Return query that relates pairs (key of data to be classified k, class c)
# to log( P(C = c) * P(A = a(k) | C = c) ). Moreover, each pair (k,c) is related
# to NULL, which serves as a default value if there is insufficient training
# data to compute a probability value.

def get_keys_and_prob_values_sql(**kwargs):
	return """
	SELECT
		classify.key,
		classPriors.class,
		CASE WHEN count(*) < {numAttrs} THEN NULL
			 ELSE
				log(classPriors.class_cnt::DOUBLE PRECISION / classPriors.all_cnt)
				+ sum( log((featureProbs.cnt::DOUBLE PRECISION + 1)
					/ (classPriors.class_cnt + featureProbs.attr_cnt)) )
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
		featureProbs.attr = classify.attr AND
		featureProbs.value = classify.value
	GROUP BY
		classify.key, classPriors.class, classPriors.class_cnt, classPriors.all_cnt
	
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


# Return query mapping each class c to log( P(C = c) * P(A = a | C = c) ) where
# A = (A_1, ..., A_n). Pass argument a in keyword parameter
# 'classifyAttrColumn'. Note that unless 'classifyAttrColumn' is a literal,
# the SQL is a correlated subquery and will not work in Greenplum.

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


# Return query mapping each key of data to be classified to the Naive Bayes
# classification.

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


# Materialize the queries returned by `get_feature_probs_sql()` and
# `get_class_priors_sql()`.
# FIXME: ANALYZE is not portable.

def create_prepared_data(**kwargs):
	"""Precompute all class priors and feature probabilities.
	
	When the precomputations are stored in a table, this function will create
	indices that speed up lookups necessary for Naive Bayes classification.
	Moreover, it runs ANALYZE on the new tables to allow for optimized query
	plans.
	
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


# Create a relation that maps each key of the data to be classified to the
# Naive Bayes classification.

def create_classification(**kwargs):
	"""
	Create a view/table that maps primary keys to classification.
	"""
	
	init_prepared_data(kwargs)
	kwargs.update(dict(
		keys_and_prob_values = "(" + get_keys_and_prob_values_sql(**kwargs) + ")"
		))
	plpy.execute("""
		CREATE {whatToCreate} {destName} AS
		SELECT
			key,
			madlib.argmax(class, log_prob) AS nb_classification
		FROM {keys_and_prob_values} AS keys_and_nb_values
		GROUP BY key
		""".format(**kwargs))


# create_bayes_probabilities(**kwargs)
# ====================================
#
# Creates a ralation that maps each pair
# (key of data to be classified k, class c) to the Naive Bayes probability for
# this class.
#
# We have two numerical problems when copmuting the probabilities
# 
#                 P(C = c) * P(A = a | C = c)
#   P(C = c) = ---------------------------------    (*)
#              --
#              \   P(C = c') * P(A = a | C = c')
#              /_ 
#                c'
#                          __
# where P(A = a | C = c) = ||  P(A_i = a_i | C = c):
#                            i
# 1. P(A = a | C = c) could be a very small number not representable in
#    double-precision floating-point arithmetic.
#    -> Solution: We have log( P(C = c) * P(A = a | C = c) ) as indermediate
#       results. We will add the maximum absolute value of these intermediate
#       results to all of them. This corresponds to multiplying numerator and
#       denominator of (*) with the same factor. The "normalization" ensures
#       that the numerator of (*) can never be 0 (in FP arithmetic) for all c.
# 2. PostgreSQL raises an error in case of underflows, even when 0 is the
#    desirable outcome.
#    -> Solution: if log( P(A = a | C = c) ) < -300, we interprete
#       P(A = a | C = c) = 0. Note here that 1e-300 is about the order of
#       magnitude of the smallest double precision FP number.

def create_bayes_probabilities(**kwargs):
	"""Create table/view that maps pairs (primary key, class) to probabilities."""

	init_prepared_data(kwargs)
	kwargs.update(dict(
		keys_and_prob_values = "(" + get_keys_and_prob_values_sql(**kwargs) + ")"
		))
	plpy.execute("""
		CREATE {whatToCreate} {destName} AS
		SELECT
			key,
			class,
			nb_prob / sum(nb_prob) OVER (PARTITION BY key) AS nb_prob
		FROM
		(
			SELECT
				key,
				class,
				CASE WHEN max(log_prob) - max(max(log_prob)) OVER (PARTITION BY key) < -300 THEN 0
					 ELSE pow(10, max(log_prob) - max(max(log_prob)) OVER (PARTITION BY key))
				END AS nb_prob
			FROM
				{keys_and_prob_values} AS keys_and_nb_values
			GROUP BY
				key, class
		) AS keys_and_nb_values
		ORDER BY
			key, class
		""".format(**kwargs))


# create_classification_function(**kwargs)
# ==========================================
# 
# Creates a SQL function that maps arrays of attribute values to the Naive Bayes
# probability for this class.
# Note: Greenplum does not support executing STABLE and VOLATILE functions on
# segments. The created function can therefore only be called on the master.

def create_classification_function(**kwargs):
	"""Create a SQL function mapping arrays of attribute values to classification."""
	
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


# When the names of relations with the preprocessed training data are not
# given, we generate queries for these relations (to run them on the fly, i.e.,
# as subqueries).

def init_prepared_data(kwargs):
	if not 'classPriorsSource' in kwargs:
		kwargs.update(dict(
				classPriorsSource = "(" + get_class_priors_sql(**kwargs) + ")" 
			))
	if not 'featureProbsSource' in kwargs:
		kwargs.update(dict(
				featureProbsSource = "(" + get_feature_probs_sql(**kwargs) + ")"
			))
