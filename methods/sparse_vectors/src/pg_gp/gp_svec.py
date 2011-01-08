import plpy

"""@namespace svecs

This module implements a sparse vector data type.

\par About:

    When we use arrays of floating point numbers for various calculations, 
    we will sometimes have long runs of zeros or some other default value. 
    This is common in many kinds of applications such as scientific computing, 
    retail optimization and text processing. Each floating point number takes 
    8 bytes of storage in memory and/or disk, so saving those zeros is often 
    impractical. There are also many computations that benefit from skipping 
    over the zeros.

    Say, for example, that we have the following array of doubles in Postgres 
    stored as a "float8[]":

      '{0, 33,...40,000 zeros..., 12, 22 }'::float8[]

    This array would occupy slightly more than 320KB of memory/disk, most of 
    it zeros. Also, if we perform various operations on this, we'll be doing 
    work on 40,001 fields that aren't important. This kind of array arises 
    often in text processing, where the dictionary may have 40-100K terms 
    and each vector will store, say, the number of occurrences of each 
    dictionary word in a particular document.

    We could use the built in "Array" data type in Postgres/GP, which has 
    a bitmap for null values, but since it is not optimized for float8[], 
    or for long runs of zeros instead of nulls and the bitmap is not 
    compressed, it is a poor choice for this use case. If we stored the 
    zeros as NULL in the Postgres/GP array, then the bitmap for nulls would 
    have 5KB of bits in it to mark the nulls, which is not nearly as 
    efficient as we'd like. A simple Run Length Encoding (RLE) scheme that 
    is biased toward being efficient for zero value bitmap would result in 
    only 6 bytes for bitmap storage.

    We also want to implement vector operators for our type that take 
    advantage of an efficient sparse storage format to make computations 
    faster.

    The above motivates the design and implementation of a data type to 
    store and manipulate sparse vectors. This module implements such a 
    data type.

\par Prerequisites:

    1. Greenplum Database 3.3 or higher
    2. A C compiler

\par Installation:

    1. Make sure the Greenplum binary distribution is in the path.
    2. On a command line, execute the following to compile the library.

       	   make install

    3. Install the function and type definitions with 

           psql -d <your_db_name> -f gp_svec.sql

    4. <Optional> Run tests with 

           psql -d <your_db_name> -af gp_svec_test.sql | diff - test_output

\par To Do:

    - The current implementation only supports sparse vectors of float8
      values. We need to extending the implementation to support other 
      base types.

    - Indexing on svecs. This requires indexing capability on arrays,
      which is currently unsupported in GP.  
	
\par Usage:

    The "Sparse Vector Datatype" is named "svec" and implements a vector 
    with compressed storage of duplicate elements. We can cast an array 
    into an svec like this:

	testdb=# select ('{0, 33,...40,000 zeros..., 12, 22}'::float8[])::svec;

    This is the same vector we input, but represented internally as a pair
    of arrays: {1,1,40000,1,1}:{0,33,0,12,22}, where the first array is the
    count array and the second, the data array. So, in this case, there is 
    1 occurrence of 0, followed by 1 occurrence of 33, followed by 40,000 
    occurrences of 0, followed by 1 occurrence of 12 and 1 occurrence of 22.

    It is also possible to input the above vector directly as an svec as
    follows 
   
        testdb=# select '{1,1,40000,1,1}:{0,33,0,12,22}'::svec;

    We can use operations with svec type like <, >, *, **, /, =, +, SUM, etc, 
    and they have meanings associated with typical vector operations. 

    The plus (+) operator adds each of the terms of two vectors of same 
    dimension together. For instance, if we have a = {0,1,5} and b = {4,3,2}, 
    then a+b = {4,4,7}.  We can see this using the svec type like this:

	testdb=# select ('{0,1,5}'::float8[]::svec + 
		 	 '{4,3,2}'::float8[]::svec)::float8[];
 	float8  
	---------
 	{4,4,7}

    Without the casting into float8[] at the end, we get:

        testdb=# select '{0,1,5}'::float8[]::svec + '{4,3,2}'::float8[]::svec;
 	?column?  
	----------
 	{2,1}:{4,7}    	    	

    A vector dot product (%*%) between these two will result in a scalar 
    result of type float8. The dot product should be (0*4 + 1*3 + 5*2) = 13, 
    like this:

	testdb=# select '{0,1,5}'::float8[]::svec %*% '{4,3,2}'::float8[]::svec;
 	?column? 
	----------
       	13

    Special vector aggregate functions are also available. SUM is self 
    explanatory. VEC_COUNT_NONZERO evaluates the count of non-zero terms 
    in each column found in a set of n-dimensional svecs and returns an 
    svec with the counts. For instance, if we have {0,1,5},
    {10,0,3},{0,0,3},{0,1,0}, then VEC_COUNT_NONZERO() would return {1,2,3}:

	testdb=# create table list (a svec);
	CREATE TABLE
	testdb=# insert into list values 
		 	('{0,1,5}'::float8[]), ('{10,0,3}'::float8[]),
			('{0,0,3}'::float8[]),('{0,1,0}'::float8[]);
	INSERT 0 4
	testdb=# select VEC_COUNT_NONZERO(a)::float8[] from list;
 	vec_count_nonzero 
	-----------------
 	{1,2,3}

    NULLs in an svec are represented as NVPs (No Value Present). For example,
    we have:

    	testdb=# select '{1,2,3}:{4,NULL,5}'::svec;
	      svec        
	-------------------
 	 {1,2,3}:{4,NVP,5}

	testdb=# select '{1,2,3}:{4,NULL,5}'::svec + '{2,2,2}:{8,9,10}'::svec; 
	         ?column?         
 	--------------------------
 	 {1,2,1,2}:{12,NVP,14,15}

    The elements/subvector of an svec can be accessed as follows:

        testdb=# select svec_proj('{1,2,3}:{4,5,6}'::svec,1) + 
		 	svec_proj('{4,5,6}:{1,2,3}'::svec,15);	 
         ?column? 
	----------
                7

        testdb=# select svec_subvec('{2,4,6}:{1,3,5}'::svec, 2, 11);
	   svec_subvec   
	----------------- 
	 {1,4,5}:{1,3,5}

    The elements/subvector of an svec can be changed using the function 
    svec_change(). It takes three arguments: an m-dimensional svec sv1, a
    start index j, and an n-dimensional svec sv2 such that j + n - 1 <= m,
    and returns an svec like sv1 but with the subvector sv1[j:j+n-1] 
    replaced by sv2. An example follows:

        testdb=# select svec_change('{1,2,3}:{4,5,6}'::svec,3,'{2}:{3}'::svec);
	     svec_change     
	---------------------
	 {1,1,2,2}:{4,5,3,6}

    There are also higher-order functions for processing svecs. For example,
    the following is the corresponding function for lapply() in R.

        testdb=# select svec_lapply('sqrt', '{1,2,3}:{4,5,6}'::svec);
	                  svec_lapply                  
	-----------------------------------------------
	 {1,2,3}:{2,2.23606797749979,2.44948974278318}

    The full list of functions available for manipulating svecs are available
    in gp_svec.sql.


\par A More Extensive Example

    For a text classification example, let's assume we have a dictionary 
    composed of words in a text array:

        testdb=# create table features (a text[][]) distributed randomly;
	testdb=# insert into features values 
		    ('{am,before,being,bothered,corpus,document,i,in,is,me,
		       never,now,one,really,second,the,third,this,until}');

    We have a set of documents, also defined as arrays of words:

	testdb=# create table documents(a int,b text[]);
	CREATE TABLE
	testdb=# insert into documents values
		 	(1,'{this,is,one,document,in,the,corpus}'),
		        (2,'{i,am,the,second,document,in,the,corpus}'),
		        (3,'{being,third,never,really,bothered,me,until,now}'),
		        (4,'{the,document,before,me,is,the,third,document}');

    Now we have a dictionary and some documents, we would like to do some 
    document categorization using vector arithmetic on word counts and 
    proportions of dictionary words in each document.

    To start this process, we'll need to find the dictionary words in each 
    document. We'll prepare what is called a Sparse Feature Vector or SFV 
    for each document. An SFV is a vector of dimension N, where N is the 
    number of dictionary words, and in each SFV is a count of each dictionary 
    word in the document.

    Inside the sparse vector library, we have a function that will create 
    an SFV from a document, so we can just do this:

	testdb=# select gp_extract_feature_histogram(
		  	      (select a from features limit 1),b)::float8[] 
	         from documents;

             	gp_extract_feature_histogram             
	-----------------------------------------
 	{0,0,0,0,1,1,0,1,1,0,0,0,1,0,0,1,0,1,0}
 	{0,0,1,1,0,0,0,0,0,1,1,1,0,1,0,0,1,0,1}
 	{1,0,0,0,1,1,1,1,0,0,0,0,0,0,1,2,0,0,0}
 	{0,1,0,0,0,2,0,0,1,1,0,0,0,0,0,2,1,0,0}

    Note that the output of gp_extract_feature_histogram is an svec for each 
    document containing the count of each of the dictionary words in the 
    ordinal positions of the dictionary. This can more easily be understood 
    by lining up the feature vector and text like this:

	testdb=# select gp_extract_feature_histogram(
				(select a from features limit 1),b)::float8[]
                        , b 
                 from documents;

	testdb=# select * from features;

    Now when we look at the document "i am the second document in the corpus", 
    its SFV is {1,3*0,1,1,1,1,6*0,1,2}. The word "am" is the first ordinate in 
    the dictionary and there is 1 instance of it in the SFV. The word "before" 
    has no instances in the document, so its value is "0" and so on.

    The function gp_extract_feature_histogram() is very speed optimized - in 
    essence a single routine version of a hash join, and should be able to 
    process large numbers of documents into their SFVs in parallel at the 
    highest possible speeds.

    The rest of the categorization process is all vector math. The actual 
    count is hardly ever used.  Instead, it's turned into a weight. The most 
    common weight is called tf/idf for Term Frequency / Inverse Document 
    Frequency. The calculation for a given term in a given document is 

      {#Times in document} * log {#Documents / #Documents the term appears in}.

    For instance, the term "document" in document A would have weight 
    1 * log (4/3). In document D, it would have weight 2 * log (4/3).
    Terms that appear in every document would have tf/idf weight 0, since 
    log (4/4) = log(1) = 0. (Our example has no term like that.) That 
    usually sends a lot of values to 0.

    For this part of the processing, we'll need to have a sparse vector of 
    the dictionary dimension (19) with the values 

    	(log(#documents/#Documents each term appears in). 

    There will be one such vector for the whole list of documents (aka the 
    "corpus"). The #documents is just a count of all of the documents, in 
    this case 4, but there is one divisor for each dictionary word and its 
    value is the count of all the times that word appears in the document. 
    This single vector for the whole corpus can then be scalar product 
    multiplied by each document SFV to produce the Term Frequency/Inverse 
    Document Frequency.

    This can be done as follows:

	testdb=# create table corpus as 
		   (select a,gp_extract_feature_histogram(
		      (select a from features limit 1),b) sfv from documents);
	testdb=# select a docnum, (sfv * logidf) tf_idf 
		 from (select log(count(sfv)/vec_count_nonzero(sfv)) logidf 
		       from corpus) foo, corpus order by docnum;

  docnum |                tf_idf                                     
  -------+---------------------------------------------------------------
       1 | {4,1,1,1,2,3,1,2,1,1,1,1}:{0,0.69,0.28,0,0.69,0,1.38,0,0.28,0,1.38,0}
       2 | {1,3,1,1,1,1,6,1,1,3}:{1.38,0,0.69,0.28,1.38,0.69,0,1.38,0.57,0}
       3 | {2,2,5,1,2,1,1,2,1,1,1}:{0,1.38,0,0.69,1.38,0,1.38,0,0.69,0,1.38}
       4 | {1,1,3,1,2,2,5,1,1,2}:{0,1.38,0,0.57,0,0.69,0,0.57,0.69,0}

    We can now get the "angular distance" between one document and the rest 
    of the documents using the ACOS of the dot product of the document vectors:

	testdb=# create table weights as 
		    (select a docnum, (sfv * logidf) tf_idf 
		     from (select log(count(sfv)/vec_count_nonzero(sfv)) logidf 
		     	   from corpus) foo, corpus order by docnum) 
	         distributed randomly ;

    Calculate the angular distance between the first document to each other 
    document:

	testdb=# select docnum,180.*(ACOS(dmin(1.,(tf_idf%*%testdoc)/(l2norm(tf_idf)*l2norm(testdoc))))/3.141592654) angular_distance 
		 from weights,(select tf_idf testdoc from weights where docnum = 1 LIMIT 1) foo 
		 order by 1;

 	docnum | angular_distance 
       --------+------------------
             1 |                0
             2 | 78.8235846096986
             3 | 89.9999999882484
             4 | 80.0232034288617

    We can see that the angular distance between document 1 and itself 
    is 0 degrees and between document 1 and 3 is 90 degrees because they 
    share no features at all. The angular distance can now be plugged into
    machine learning algorithms that rely only on a distance measure between
    points.


