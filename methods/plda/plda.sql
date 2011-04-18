/* ----------------------------------------------------------------------- *//** 
 *
 * @file plda.sql
 *
 * @brief SQL functions for parallel Latent Dirichlet Allocation
 * @author Kee Siong Ng
 *
 * @sa For an introduction to Latent Dirichlet Allocation models, see the 
       module description \ref grp_lda.
 *
 *//* ------------------------------------------------------------------------*/

/**

@addtogroup grp_lda

@about
\section about About

Latent Dirichlet Allocation (LDA) is an interesting generative probabilistic 
model for natural texts that has received a lot of attention in recent years. 
The model is quite versatile, having found uses in problems like automated 
topic discovery, collaborative filtering, and document classification.

The LDA model posits that each document is associated with a mixture of various 
topics (e.g. a document is related to Topic 1 with probability 0.7, and Topic 2 with 
probability 0.3), and that each word in the document is attributable to one 
of the document's topics. There is a (symmetric) Dirichlet prior with parameter 
\f$ \alpha \f$ on each document's topic mixture. In addition, there is another 
(symmateric) Dirichlet prior with parameter \f$ \eta \f$ on the distribution 
of words for each topic. The following generative process then defines a distribution 
over a corpus of documents. First sample, for each topic \f$ i \f$, a per-topic word distribution 
\f$ \Phi_i \f$ from the Dirichlet(\f$\eta\f$) prior. 
Then for each document:
-# Sample a document length N from a suitable distribution, say, Poisson.
-# Sample a topic mixture \f$ \theta \f$ for the document from the Dirichlet(\f$\alpha\f$) distribution.
-# For each of the N words:
   -# Sample a topic \f$ z_n \f$ from the multinomial topic distribution \f$ \theta \f$.
   -# Sample a word \f$ w_n \f$ from the multinomial word distribution \f$ \Phi_{z_n} \f$ associated with topic \f$ z_n \f$.

In practice, only the words in each document are observable. The topic mixture of 
each document and the topic for each word in each document are latent unobservable 
variables that need to be inferred from the observables, and this is the problem
people refer to when they talk about the inference problem for LDA. Exact inference
is intractable, but several approximate inference algorithms for LDA have been
developed. The simple and effective Gibbs sampling algorithm described in 
Griffiths and Steyvers [2] appears to be the current algorithm of choice. Our 
parallel implementation of LDA comes from Wang et al [3], which is essentially
a straightforward parallelisation of the Gibbs sampling algorithm.

See also http://code.google.com/p/plda/.

@installation
\section installation Installation

   To install the module, run the following command in the plda source directory to compile 
   the shared object file.
   \code
   make install
   \endcode
   If the machine is a multi-node machine, we need to copy plda_support.so to all the segment
   hosts as follows (you may need to do this as gpadmin):
   \code
   gpscp -f /path/to/host_file plda_support.so =:$GPHOME/lib/postgresql/
   \endcode
   We can then install the parallel LDA user-defined functions as follows:
   \code
   psql db_name -f plda.sql
   \endcode
   To uninstall the parallel LDA module, we do this:
   \code
   psql db_name -f plda_drop.sql
   \endcode

@usage
\section usage Application Interface

This is the main learning function.

-  Topic inference is achieved through the following UDF
   \code
   lda_run(datatable, dicttable, modeltable, outputdatatable, 
           numiter int, numtopics int, alpha float8, eta float8),
   \endcode
   where datatable is the table where the corpus to be analysed is stored (the table is
   assumed to have the two columns: id int and contents int[]), dicttable is the table
   where the dictionary is stored (the table is assumed to have the column dict text[],
   and contain only one row), modeltable is the name of the table the system will store the
   learned model (in the form of word-topic counts), outputdatatable is the name of the table
   the system will store a copy of the datatable plus topic assignments to each document,
   numiter is the number of iterations to run the Gibbs sampling, 
   numtopics is the number of topics to discover, alpha is the parameter to the topic Dirichlet
   prior, and eta is the parameter to the Dirichlet prior on the per-topic word distributions.

There is another function that will allow users to predict the labels of new documents using
the LDA model learned from a training corpus. This function, implemented as 
madlib.lda_label_test_documents() in plda.sql, is still subject to further testing and is to 
be used with caution for now.

@examp
\section example Example

We now give a usage example.

-# As a first step, we need to prepare and populate two tables/views with the following 
   structure:
   \code   
   corpus_table_name (       
         id         INT4,    -- document ID
         contents   INT4[],  -- words in the document
	 ...                 -- other fields
   );
   dictionary_table_name ( 
         dict       TEXT[],   -- words in the dictionary
         ...                  -- other fields
   );
   \endcode
   Words must be represented using positive numbers.
   The module comes with some randomly generated data. For example, we can import two
   test tables madlib.lda_mycorpus and madlib.lda_mydict using the command
   \code
   psql database_name -f importTestcases.sql
   \endcode     
-# To run the program, we need to call the lda_run() function with the appropriate parameters. 
   The file plda_test.sql contains an example of a call sequence. After making the 
   necessary changes to the desired parameters for lda_run(), we can kick-start the 
   inference process by running the following:
   \code
   psql database_name -f plda_test.sql
   \endcode

After a successful run of the lda_run() function, the most probable words associated 
with each topic are displayed. Other results of the learning process can be obtained 
by running the following commands inside the Greenplum Database. Here we assume
the modeltable and outputdatatable parameters to lda_run() are 'madlib.lda_mymodel'
and 'madlib.lda_corpus' respectively.

-# The topic assignments for each document can be obtained as follows:
   \code
   select id, (topics).topics from madlib.lda_corpus;
   \endcode
-# The topic distribution of each document can be obtained as follows:
   \code
   select id, (topics).topic_d from madlib.lda_corpus;
   \endcode
-# The number of times each word was assigned to a given topic in the whole corpus can 
   be computed as follows:
   \code
   select ss.i, madlib.lda_word_topic_distrn(gcounts,$numtopics,ss.i) 
   from madlib.lda_mymodel, 
        (select generate_series(1,$dictsize) i) as ss;
   \endcode
   where $numtopics is the number of topics used in the learning process, and 
   $dictsize is the size of the dictionary.
-# The total number of words assigned to each topic in the whole corpus can be computed 
   as follows:
   \code
   select sum((topics).topic_d) topic_sums from madlib.lda_corpus;
   \endcode

\section notes Additional Notes

The input format for the Parallel LDA module is different from that used by the 
`lda' package for R. In the `lda' package, each document is represented by two
equal dimensional integer arrays. The first array represents the words that occur
in the document, and the second array captures the number of times each word in
the first array occurs in the document.
This representation appears to have a major weakness in that all the occurrences
of a word in a document must be assigned the same topic, which is clearly not
satisfactory. Further, at the time of writing, the main learning function in the 
`lda' package actually does not work correctly when the occurrence counts for words 
aren't one.

There is a script called generateTestCases.cc that can be used to generate some
simple test documents to validate the correctness and effiency of the parallel
LDA implementation.

This module is not yet in the main MADlib repository. It currently sits in the 
madlib/contrib directory and will be moved into the main madlib directory over
time.
 
@literature
\section literature Literature

Here are some relevant references.

[1] D.M. Blei, A.Y. Ng, M.I. Jordan, <em>Latent Dirichlet Allocation</em>, 
    Journal of Machine Learning Research, vol. 3, pp. 993-1022, 2003.

[2] T. Griffiths and M. Steyvers, <em>Finding scientific topics</em>, 
    PNAS, vol. 101, pp. 5228-5235, 2004.

[3] Y. Wang, H. Bai, M. Stanton, W-Y. Chen, and E.Y. Chang, <em>PLDA: 
    Parallel Dirichlet Allocation for Large-scale Applications</em>, AAIM, 2009.

[4] http://en.wikipedia.org/wiki/Latent_Dirichlet_allocation

[5] J. Chang, Collapsed Gibbs sampling methods for topic models, R manual, 2010.

*/

-- \i plda_drop.sql

-- The lda_topics_t data type store the assignment of topics to each word in a document,
-- plus the distribution of those topics in the document.
CREATE TYPE madlib.lda_topics_t AS (
       topics int4[],
       topic_d int4[]
);

-- Returns a zero'd array of a given dimension
CREATE OR REPLACE FUNCTION madlib.lda_zero_array(d int4) RETURNS int4[] 
AS 'plda_support.so', 'zero_array' LANGUAGE C STRICT;

-- Returns an array of random topic assignments for a given document length
CREATE OR REPLACE FUNCTION madlib.lda_random_topics(doclen int4, numtopics int4) RETURNS madlib.lda_topics_t 
AS 'plda_support.so', 'randomTopics' LANGUAGE C STRICT;

-- This function assigns a randomly chosen topic to each word in a document according to 
-- the count statistics obtained for the document and the whole corpus so far. 
-- Parameters
--   doc     : the document to be analysed
--   topics  : the topic of each word in the doc
--   topic_d : the topic distribution for the doc
--   global_count : the global word-topic counts
--   topic_counts : the counts of all words in the corpus in each topic
--   num_topics   : number of topics to be discovered
--   dsize        : size of dictionary
--   alpha        : the parameter of the Dirichlet distribution
--   eta          : the parameter of the Dirichlet distribution
--
CREATE OR REPLACE FUNCTION
madlib.lda_sample_new_topics(doc int4[], topics int4[], topic_d int4[], global_count int4[],
		       	     topic_counts int4[], num_topics int4, dsize int4, alpha float, eta float) 
RETURNS madlib.lda_topics_t
AS 'plda_support.so', 'sampleNewTopics' LANGUAGE C STRICT;

-- Computes the per document word-topic counts
CREATE OR REPLACE FUNCTION 
madlib.lda_cword_count(mystate int4[], doc int4[], topics int4[], doclen int4, num_topics int4, dsize int4)
RETURNS int4[]
AS 'plda_support.so', 'cword_count' LANGUAGE C;

-- Aggregate function to compute all word-topic counts given topic assignments for each document
CREATE AGGREGATE madlib.lda_cword_agg(int4[], int4[], int4, int4, int4) (
       sfunc = madlib.lda_cword_count,
       stype = int4[] 
);

-- The main parallel LDA learning function
CREATE OR REPLACE FUNCTION
madlib.lda_train(num_topics int4, num_iter int4, init_iter int4, alpha float, eta float, 
	         data_table text, dict_table text, model_table text, output_data_table text) 
RETURNS int4 AS $$
	# -- Get dictionary size
	dsize_t = plpy.execute("SELECT array_upper(dict,1) dsize FROM " + dict_table)
	dsize = dsize_t[0]['dsize']

	if (dsize == 0):
	    plpy.error("error: dictionary has not been initialised")

	# -- Initialise global word-topic counts 
	plpy.info('Initialising global word-topic count')
	glwcounts_t = plpy.execute("SELECT madlib.lda_zero_array(" + str(dsize*num_topics) + ") glwcounts")
	glwcounts = glwcounts_t[0]['glwcounts']

	# -- This stores the local word-topic counts computed at each segment 
	plpy.execute("CREATE TEMP TABLE local_word_topic_count ( id int4, mytimestamp int4, lcounts int4[] ) " + 
                     "DISTRIBUTED BY (mytimestamp)")

	# -- This stores the global word-topic counts	     
	plpy.execute("CREATE TABLE " + model_table + " ( mytimestamp int4, gcounts int4[] ) " +
		     "DISTRIBUTED BY (mytimestamp)")	     

	# -- Copy corpus into temp table
	plpy.execute("CREATE TEMP TABLE corpus0" + " ( id int4, contents int4[], topics madlib.lda_topics_t ) " +
		     "WITH (appendonly=true, orientation=column, compresstype=quicklz) DISTRIBUTED RANDOMLY")

	plpy.execute("INSERT INTO corpus0 (SELECT id, contents, madlib.lda_random_topics(array_upper(contents,1)," + str(num_topics) + 
			     	  	  ") FROM " + data_table + ")")

	# -- Get topic counts				  
	topic_counts_t = plpy.execute("SELECT sum((topics).topic_d) tc FROM corpus0")
	topic_counts = topic_counts_t[0]['tc']

	for i in range(1,num_iter+1):
	    iter = i + init_iter

	    new_table_id = i % 2
	    if (new_table_id == 0):
	         old_table_id = 1
	    else:
		 old_table_id = 0	 

	    plpy.execute("CREATE TEMP TABLE corpus" + str(new_table_id) + 
	    	         " ( id int4, contents int4[], topics madlib.lda_topics_t ) " + 
			 "WITH (appendonly=true, orientation=column, compresstype=quicklz) DISTRIBUTED RANDOMLY")

	    # -- Randomly reassign topics to the words in each document, in parallel; the map step
	    plpy.execute("INSERT INTO corpus" + str(new_table_id) 
	    		 + " (SELECT id, contents, madlib.lda_sample_new_topics(contents,(topics).topics,(topics).topic_d,'" 
			     	 	            + str(glwcounts) + "','" + str(topic_counts) + "'," + str(num_topics) 
					            + "," + str(dsize) + "," + str(alpha) + "," + str(eta) + ") FROM corpus" + str(old_table_id) + ")")

	    plpy.execute("DROP TABLE corpus" + str(old_table_id)) 
    
	    # -- Compute the local word-topic counts in parallel; the map step
	    plpy.execute("INSERT INTO local_word_topic_count " +
	    		 " (SELECT gp_segment_id, " + str(iter) 
			     	   + ", madlib.lda_cword_agg(contents,(topics).topics,array_upper(contents,1)," 
				   + str(num_topics) + "," + str(dsize) + ") FROM corpus" + str(new_table_id) + 
			    " GROUP BY gp_segment_id)")  

	    # -- Compute the global word-topic counts; the reduce step; Is storage into model_table necessary?
	    plpy.execute("INSERT INTO " + model_table +
	    		 " (SELECT " + str(iter) + ", sum(lcounts) FROM local_word_topic_count " +
			  " WHERE mytimestamp = " + str(iter) + ")")
	    
	    glwcounts_t = plpy.execute("SELECT gcounts[1:" + str(dsize*num_topics) + "] glwcounts " +
	    		  	       "FROM " + model_table + " WHERE mytimestamp = " + str(iter))
	    glwcounts = glwcounts_t[0]['glwcounts']

	    # -- Compute the denominator
	    topic_counts_t = plpy.execute("SELECT sum((topics).topic_d) tc FROM corpus" + str(new_table_id))
	    topic_counts = topic_counts_t[0]['tc']

	    if (iter % 5 == 0):
	         plpy.info('  Done iteration %d' % iter)

	# -- This store a copy of the corpus of documents to be analysed
	plpy.execute("CREATE TABLE " + output_data_table + 
	             "( id int4, contents int4[], topics madlib.lda_topics_t ) DISTRIBUTED RANDOMLY")
	plpy.execute("INSERT INTO " + output_data_table + " (SELECT * FROM corpus" + str(new_table_id) + ")")

	# -- Clean up    
	plpy.execute("DROP TABLE corpus" + str(new_table_id))
	plpy.execute("DROP TABLE local_word_topic_count")
	plpy.execute("DELETE FROM " + model_table + " WHERE mytimestamp < " + str(num_iter))

	return init_iter + num_iter   
$$ LANGUAGE plpythonu;

CREATE TYPE madlib.lda_word_weight AS ( word text, prob float, wcount int4 );

-- Returns the most important words for each topic, base on Pr( word | topic ).
CREATE OR REPLACE FUNCTION 
madlib.lda_topic_word_prob(num_topics int4, mytopic int4, ltimestamp int4, model_table text, data_table text, dict_table text) 
RETURNS SETOF madlib.lda_word_weight AS $$
       dict_t = plpy.execute("SELECT dict FROM " + dict_table)
       if (dict_t.nrows() == 0):
       	   plpy.error("error: failed to find dictionary")
       dict = dict_t[0]['dict']
       dict = map(int, dict[1:-1].split(',')) # -- convert string into array
       dsize = len(dict)

       glbcounts_t = plpy.execute("SELECT gcounts[1:" + str(dsize*num_topics) + "] glbcounts FROM " + model_table + 
       		     		  " WHERE mytimestamp = " + str(ltimestamp))
       if (glbcounts_t.nrows() == 0):
       	   plpy.error("error: failed to find gcounts for time step %d" % ltimestamp)
       glbcounts = glbcounts_t[0]['glbcounts']

       topic_sum_t = plpy.execute("SELECT sum((topics).topic_d) topic_sum FROM " + data_table)
       if (topic_sum_t.nrows() == 0):
       	   plpy.error("error: failed to compute topic_sum")
       topic_sum = topic_sum_t[0]['topic_sum']

       # -- Convert the strings into arrays
       glbcounts = map(int, glbcounts[1:-1].split(','))
       topic_sum = map(int, topic_sum[1:-1].split(','))

       ret = []
       for i in range(0,dsize):
       	   idx = i*num_topics + mytopic - 1
       	   wcount = glbcounts[idx]
	   prob = wcount * 1.0 / topic_sum[mytopic - 1]
	   ret = ret + [(dict[i],prob,wcount)]

       return ret	   
$$ LANGUAGE plpythonu;


CREATE TYPE madlib.lda_word_distrn AS ( word text, distrn int4[], prob float8[] );

-- This function computes the topic assignments to words in a document given previously computed
-- statistics from the training corpus. This function and the next are still being tested.
-- This function has not been rewritten in PL/Python because it is hard to handle structured types
-- like madlib.lda_topics_t in PL/Python (this has to be done via conversion to text, which is not nice).
CREATE OR REPLACE FUNCTION 
madlib.lda_label_document(doc int4[], global_count int4[], topic_counts int4[], num_topics int4, dsize int4, 
		     alpha float, eta float)
RETURNS madlib.lda_topics_t AS $$
DECLARE
	ret madlib.lda_topics_t;
BEGIN
	ret := madlib.lda_random_topics(array_upper(doc,1), num_topics);
	FOR i in 1..20 LOOP
	    ret := madlib.lda_sample_new_topics(doc,(ret).topics,(ret).topic_d,global_count,topic_counts,num_topics,dsize,alpha,eta);
    	END LOOP;
	RETURN ret;
END;
$$ LANGUAGE plpgsql;

-- This function computes the topic assignments to documents in a test corpus.
-- The data_table argument appears unnecessary, as long as topic_counts is saved in a table from the lda_train() routine
CREATE OR REPLACE FUNCTION
madlib.lda_label_test_documents(test_table text, model_table text, data_table text, dict_table text, ltimestep int4, num_topics int4, alpha float, eta float)
RETURNS VOID AS $$
	dsize_t = plpy.execute("SELECT array_upper(dict,1) dictsize FROM " + dict_table)
	dsize = dsize_t[0]['dictsize']

        # Get the word-topic counts
	glwcounts_t = plpy.execute("SELECT gcounts[1:" + str(dsize*num_topics) + "] glwcounts FROM " 
                                   + model_table + " WHERE mytimestamp = " + str(ltimestep))
	if (glwcounts_t.nrows() == 0):
	    plpy.error("error: gcounts for time step %d not found" % ltimestep)
	glwcounts = glwcounts_t[0]['glwcounts']

        # Get counts for each topic; this appears heavy, should be saved from lda_train() routine
	topic_counts_t = plpy.execute("SELECT sum((topics).topic_d) tc FROM " + data_table)
	if (topic_counts_t.nrows() == 0):
	    plpy.error("error: failed to compute topic_counts")
	topic_counts = topic_counts_t[0]['tc']

        # Compute new topic assignments for each document
	plpy.execute("UPDATE " + test_table 
                     + " SET topics = madlib.lda_label_document(contents, '" 
                                        + str(glwcounts) + "', '" + str(topic_counts) + "', " 
                                        + str(num_topics) + ", " + str(dsize) + ", " 
                                        + str(alpha) + ", " + str(eta) + ")")
$$ LANGUAGE plpythonu;

-- Returns the distribution of topics for a given word
CREATE OR REPLACE FUNCTION madlib.lda_word_topic_distrn(arr int4[], ntopics int4, word int4, OUT ret int4[]) AS $$
       SELECT $1[(($3-1)*$2 + 1):(($3-1)*$2 + $2)];
$$ LANGUAGE sql;


CREATE OR REPLACE FUNCTION 
madlib.lda_run(datatable text, dicttable text, modeltable text, outputdatatable text, 
	       numiter int4, numtopics int4, alpha float, eta float)
RETURNS VOID AS $$
    restartstep = 0
    plpy.execute("SELECT setseed(0.5)")
        
    plpy.info('Starting learning process')
    plpy.execute("SELECT madlib.lda_train(" + str(numtopics) + "," 
                               + str(numiter) +"," + str(restartstep) + "," 
                               + str(alpha) + "," + str(eta) + ",'" + datatable + "', '" 
			       + dicttable + "','" + modeltable + "','" + outputdatatable + "')")

    # -- Print the most probable words in each topic
    for i in range(1,numtopics+1):
        rv = plpy.execute("select * from madlib.lda_topic_word_prob(" 
                                            + str(numtopics) + "," + str(i) + "," + str(numiter) 
                                            + ", '" + modeltable + "', '" + outputdatatable + "', '" 
                                            + dicttable + "') order by -prob limit 20")
        plpy.info( 'Topic %d' % i)
        for j in range(0,min(len(rv),20)):
            word = rv[j]['word']
            prob = rv[j]['prob']
            count = rv[j]['wcount']
            plpy.info( ' %d) %s    %f  %d' % (j+1, word, prob, count))

$$ LANGUAGE plpythonu;


