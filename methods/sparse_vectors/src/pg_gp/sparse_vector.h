/**
 * @file
 * \brief Persistent storage for the Sparse Vector Datatype
 *
 */

/*! \defgroup sparse_vectors

\par About:

This module implements a sparse vector data type named "svec", which 
gives compressed storage of sparse vectors with many duplicate elements.

When we use arrays of floating point numbers for various calculations, 
    we will sometimes have long runs of zeros (or some other default value). 
    This is common in applications like scientific computing, 
    retail optimization, and text processing. Each floating point number takes 
    8 bytes of storage in memory and/or disk, so saving those zeros is often 
    worthwhile. There are also many computations that can benefit from skipping
    over the zeros.

    To focus the discussion, consider, for example, the following 
    array of doubles stored as a Postgres/GP "float8[]" data type:

\code
      '{0, 33,...40,000 zeros..., 12, 22 }'::float8[].
\endcode

    This array would occupy slightly more than 320KB of memory/disk, most of 
    it zeros. Even if we were to exploit the null bitmap and store the zeros 
    as nulls, we would still end up with a 5KB null bitmap, which is still 
    not nearly as memory efficient as we'd like. Also, as we perform various 
    operations on the array, we'll often be doing work on 40,000 fields that 
    would turn out not to be important. 

    To solve the problems associated with the processing of sparse vectors 
    discussed above, we adopt a simple Run Length Encoding (RLE) scheme to 
    represent sparse vectors as pairs of count-value arrays. So, for example, 
    the array above would be represented as follows

\code
      '{1,1,40000,1,1}:{0,33,0,12,22}'::svec,
\endcode

    which says there is 1 occurrence of 0, followed by 1 occurrence of 33, 
    followed by 40,000 occurrences of 0, etc. In contrast to the naive 
    representations, we only need 5 integers and 5 floating point numbers
    to store the array. Further, it is easy to implement vector operations 
    that can take advantage of the RLE representation to make computations 
    faster. The module provides a library of such functions.

\par Prerequisites

    -# Greenplum Database 3.3 or higher
    -# A C compiler

\par Installation

    -# Make sure the Greenplum binary distribution is in the path.
    -# On a command line, execute the following to compile the library.

\code
       	   make install
\endcode

    -# Install the function and type definitions with 

\code
           psql -d <your_db_name> -f gp_svec.sql
\endcode

    -# <Optional> Run tests with 

\code
           psql -d <your_db_name> -af gp_svec_test.sql | diff - test_output
\endcode

\par Todo

    - The current implementation only supports sparse vectors of float8
      values. Future versions will support other base types.

    - Indexing on svecs. This requires indexing capability on arrays,
      which is currently unsupported in GP.  

\par Usage

    We can input an array directly as an svec as follows: 
\code   
    testdb=# select '{1,1,40000,1,1}:{0,33,0,12,22}'::svec.
\endcode
    We can also cast an array into an svec:
\code
    testdb=# select ('{0,33,...40,000 zeros...,12,22}'::float8[])::svec.
\endcode
    We can use operations with svec type like <, >, *, **, /, =, +, SUM, etc, 
    and they have meanings associated with typical vector operations. For 
    example, the plus (+) operator adds each of the terms of two vectors having
    the same dimension together. 
\code
    testdb=# select ('{0,1,5}'::float8[]::svec + '{4,3,2}'::float8[]::svec)::float8[];
     float8  
    ---------
     {4,4,7}
\endcode

    Without the casting into float8[] at the end, we get:
\code
    testdb=# select '{0,1,5}'::float8[]::svec + '{4,3,2}'::float8[]::svec;
     ?column?  
    ----------
    {2,1}:{4,7}    	    	
\endcode

    A dot product (%*%) between the two vectors will result in a scalar 
    result of type float8. The dot product should be (0*4 + 1*3 + 5*2) = 13, 
    like this:
\code
    testdb=# select '{0,1,5}'::float8[]::svec %*% '{4,3,2}'::float8[]::svec;
     ?column? 
    ----------
        13
\endcode

    Special vector aggregate functions are also available. SUM is self 
    explanatory. VEC_COUNT_NONZERO evaluates the count of non-zero terms 
    in each column found in a set of n-dimensional svecs and returns an 
    svec with the counts. For instance, if we have the vectors {0,1,5},
    {10,0,3},{0,0,3},{0,1,0}, then executing the VEC_COUNT_NONZERO() aggregate
    function would result in {1,2,3}:

\code
    testdb=# create table list (a svec);
    testdb=# insert into list values ('{0,1,5}'::float8[]), ('{10,0,3}'::float8[]), ('{0,0,3}'::float8[]),('{0,1,0}'::float8[]);
 
    testdb=# select VEC_COUNT_NONZERO(a)::float8[] from list;
    vec_count_nonzero 
    -----------------
        {1,2,3}
\endcode

    We do not use null bitmaps in the svec data type. A null value in an svec 
    is represented explicitly as an NVP (No Value Present) value. For example, 
    we have:
\code
    testdb=# select '{1,2,3}:{4,null,5}'::svec;
          svec        
    -------------------
     {1,2,3}:{4,NVP,5}

    testdb=# select '{1,2,3}:{4,null,5}'::svec + '{2,2,2}:{8,9,10}'::svec; 
             ?column?         
     --------------------------
      {1,2,1,2}:{12,NVP,14,15}
\endcode

    An element of an svec can be accessed using the svec_proj() function,
    which takes an svec and the index of the element desired.
\code
    testdb=# select svec_proj('{1,2,3}:{4,5,6}'::svec, 1) + svec_proj('{4,5,6}:{1,2,3}'::svec, 15);     
     ?column? 
    ----------
        7
\endcode

    A subvector of an svec can be accessed using the svec_subvec() function,
    which takes an svec and the start and end index of the subvector desired.
\code
    testdb=# select svec_subvec('{2,4,6}:{1,3,5}'::svec, 2, 11);
       svec_subvec   
    ----------------- 
     {1,4,5}:{1,3,5}
\endcode

    The elements/subvector of an svec can be changed using the function 
    svec_change(). It takes three arguments: an m-dimensional svec sv1, a
    start index j, and an n-dimensional svec sv2 such that j + n - 1 <= m,
    and returns an svec like sv1 but with the subvector sv1[j:j+n-1] 
    replaced by sv2. An example follows:
\code
    testdb=# select svec_change('{1,2,3}:{4,5,6}'::svec,3,'{2}:{3}'::svec);
         svec_change     
    ---------------------
     {1,1,2,2}:{4,5,3,6}
\endcode

    There are also higher-order functions for processing svecs. For example,
    the following is the corresponding function for lapply() in R.
\code
    testdb=# select svec_lapply('sqrt', '{1,2,3}:{4,5,6}'::svec);
                      svec_lapply                  
    -----------------------------------------------
     {1,2,3}:{2,2.23606797749979,2.44948974278318}
\endcode

    The full list of functions available for operating on svecs are available
    in gp_svec.sql.

\par A More Extensive Example

    For a text classification example, let's assume we have a dictionary 
    composed of words in a text array:
\code
    testdb=# create table features (a text[][]) distributed randomly;
    testdb=# insert into features values 
                ('{am,before,being,bothered,corpus,document,i,in,is,me,
                   never,now,one,really,second,the,third,this,until}');
\endcode
    We have a set of documents, each represented as an array of words:
\code
    testdb=# create table documents(a int,b text[]);
    testdb=# insert into documents values
                (1,'{this,is,one,document,in,the,corpus}'),
                (2,'{i,am,the,second,document,in,the,corpus}'),
                (3,'{being,third,never,really,bothered,me,until,now}'),
                (4,'{the,document,before,me,is,the,third,document}');
\endcode

    Now we have a dictionary and some documents, we would like to do some 
    document categorization using vector arithmetic on word counts and 
    proportions of dictionary words in each document.

    To start this process, we'll need to find the dictionary words in each 
    document. We'll prepare what is called a Sparse Feature Vector or SFV 
    for each document. An SFV is a vector of dimension N, where N is the 
    number of dictionary words, and in each cell of an SFV is a count of 
    each dictionary word in the document.

    Inside the sparse vector library, we have a function that will create 
    an SFV from a document, so we can just do this:
\code
    testdb=# select gp_extract_feature_histogram((select a from features limit 1),b)::float8[] 
             from documents;

          gp_extract_feature_histogram             
    -----------------------------------------
     {0,0,0,0,1,1,0,1,1,0,0,0,1,0,0,1,0,1,0}
     {0,0,1,1,0,0,0,0,0,1,1,1,0,1,0,0,1,0,1}
     {1,0,0,0,1,1,1,1,0,0,0,0,0,0,1,2,0,0,0}
     {0,1,0,0,0,2,0,0,1,1,0,0,0,0,0,2,1,0,0}
\endcode
    Note that the output of gp_extract_feature_histogram() is an svec for each 
    document containing the count of each of the dictionary words in the 
    ordinal positions of the dictionary. This can more easily be understood 
    by lining up the feature vector and text like this:
\code
    testdb=# select gp_extract_feature_histogram((select a from features limit 1),b)::float8[]
                    , b 
             from documents;

          gp_extract_feature_histogram       |                        b                         
    -----------------------------------------+--------------------------------------------------
     {1,0,0,0,1,1,1,1,0,0,0,0,0,0,1,2,0,0,0} | {i,am,the,second,document,in,the,corpus}
     {0,1,0,0,0,2,0,0,1,1,0,0,0,0,0,2,1,0,0} | {the,document,before,me,is,the,third,document}
     {0,0,0,0,1,1,0,1,1,0,0,0,1,0,0,1,0,1,0} | {this,is,one,document,in,the,corpus}
     {0,0,1,1,0,0,0,0,0,1,1,1,0,1,0,0,1,0,1} | {being,third,never,really,bothered,me,until,now}

    testdb=# select * from features;
                                                   	a                                                    
    --------------------------------------------------------------------------------------------------------
    {am,before,being,bothered,corpus,document,i,in,is,me,never,now,one,really,second,the,third,this,until}
\endcode

    Now when we look at the document "i am the second document in the corpus", 
    its SFV is {1,3*0,1,1,1,1,6*0,1,2}. The word "am" is the first ordinate in 
    the dictionary and there is 1 instance of it in the SFV. The word "before" 
    has no instances in the document, so its value is "0" and so on.

    The function gp_extract_feature_histogram() is optimized for speed -- in 
    essence a single routine version of a hash join -- and can process large 
    numbers of documents into their SFVs in parallel at high speed.

    The rest of the categorization process is all vector math. The actual 
    count is hardly ever used.  Instead, it's turned into a weight. The most 
    common weight is called tf/idf for Term Frequency / Inverse Document 
    Frequency. The calculation for a given term in a given document is 
\code
      {#Times in document} * log {#Documents / #Documents the term appears in}.
\endcode
    For instance, the term "document" in document A would have weight 
    1 * log (4/3). In document D, it would have weight 2 * log (4/3).
    Terms that appear in every document would have tf/idf weight 0, since 
    log (4/4) = log(1) = 0. (Our example has no term like that.) That 
    usually sends a lot of values to 0.

    For this part of the processing, we'll need to have a sparse vector of 
    the dictionary dimension (19) with the values 
\code
        log(#documents/#Documents each term appears in). 
\endcode
    There will be one such vector for the whole list of documents (aka the 
    "corpus"). The #documents is just a count of all of the documents, in 
    this case 4, but there is one divisor for each dictionary word and its 
    value is the count of all the times that word appears in the document. 
    This single vector for the whole corpus can then be scalar product 
    multiplied by each document SFV to produce the Term Frequency/Inverse 
    Document Frequency weights.

    This can be done as follows:
\code
    testdb=# create table corpus as 
                (select a,gp_extract_feature_histogram((select a from features limit 1),b) sfv 
		 from documents);
    testdb=# select a docnum, (sfv * logidf) tf_idf 
             from (select log(count(sfv)/vec_count_nonzero(sfv)) logidf 
                   from corpus) foo, corpus order by docnum;

    docnum |                tf_idf                                     
    -------+----------------------------------------------------------------------
         1 | {4,1,1,1,2,3,1,2,1,1,1,1}:{0,0.69,0.28,0,0.69,0,1.38,0,0.28,0,1.38,0}
         2 | {1,3,1,1,1,1,6,1,1,3}:{1.38,0,0.69,0.28,1.38,0.69,0,1.38,0.57,0}
         3 | {2,2,5,1,2,1,1,2,1,1,1}:{0,1.38,0,0.69,1.38,0,1.38,0,0.69,0,1.38}
         4 | {1,1,3,1,2,2,5,1,1,2}:{0,1.38,0,0.57,0,0.69,0,0.57,0.69,0}
\endcode

    We can now get the "angular distance" between one document and the rest 
    of the documents using the ACOS of the dot product of the document vectors:
\code
    testdb=# create table weights as 
                (select a docnum, (sfv * logidf) tf_idf 
                 from (select log(count(sfv)/vec_count_nonzero(sfv)) logidf 
                       from corpus) foo, corpus order by docnum) 
             distributed randomly ;
\endcode
    The following calculates the angular distance between the first document 
    and each of the other documents:
\code
    testdb=# select docnum,180.*(ACOS(dmin(1.,(tf_idf%*%testdoc)/(l2norm(tf_idf)*l2norm(testdoc))))/3.141592654) angular_distance 
             from weights,(select tf_idf testdoc from weights where docnum = 1 LIMIT 1) foo 
             order by 1;

    docnum | angular_distance 
   --------+------------------
         1 |                0
         2 | 78.8235846096986
         3 | 89.9999999882484
         4 | 80.0232034288617
\endcode
    We can see that the angular distance between document 1 and itself 
    is 0 degrees and between document 1 and 3 is 90 degrees because they 
    share no features at all. The angular distance can now be plugged into
    machine learning algorithms that rely on a distance measure between
    data points.

    Other extensive examples of svecs usage can be found in the k-means
    module.

*/

#ifndef SPARSEVECTOR_H
#define SPARSEVECTOR_H

#include "SparseData.h"
#include "float_specials.h"

/**
 * Consists of the dimension of the vector (how many elements) and a SparseData
 * structure that stores the data in a compressed format.
 */
typedef struct {
	int4 vl_len_;   /**< This is unused at the moment */
	int4 dimension; /**< Number of elements in this vector, special case is -1 indicates a scalar */
	char data[1];   /**< The serialized SparseData representing the vector here */
} SvecType;

#define DatumGetSvecTypeP(X)           ((SvecType *) PG_DETOAST_DATUM(X))
#define DatumGetSvecTypePCopy(X)       ((SvecType *) PG_DETOAST_DATUM_COPY(X))
#define PG_GETARG_SVECTYPE_P(n)        DatumGetSvecTypeP(PG_GETARG_DATUM(n))
#define PG_GETARG_SVECTYPE_P_COPY(n)   DatumGetSvecTypePCopy(PG_GETARG_DATUM(n))
#define PG_RETURN_SVECTYPE_P(x)        PG_RETURN_POINTER(x)

/* Below are the locations of the SparseData values within the serialized
 * inline SparseData below the Svec header
 *
 * All macros take an (SvecType *) as argument
 */
#define SVECHDRSIZE	(VARHDRSZ + sizeof(int4))
/* Beginning of the serialized SparseData */
#define SVEC_SDATAPTR(x)	((char *)(x)+SVECHDRSIZE)
#define SVEC_SIZEOFSERIAL(x)	(SVECHDRSIZE+SIZEOF_SPARSEDATASERIAL((SparseData)SVEC_SDATAPTR(x)))
#define SVEC_UNIQUE_VALCNT(x)	(SDATA_UNIQUE_VALCNT(SVEC_SDATAPTR(x)))
#define SVEC_TOTAL_VALCNT(x)	(SDATA_TOTAL_VALCNT(SVEC_SDATAPTR(x)))
#define SVEC_DATA_SIZE(x) 	(SDATA_DATA_SIZE(SVEC_SDATAPTR(x)))
#define SVEC_VALS_PTR(x)	(SDATA_VALS_PTR(SVEC_SDATAPTR(x)))
/* The size of the index is variable unlike the values, so in the serialized 
 * SparseData, we include an int32 that indicates the size of the index.
 */
#define SVEC_INDEX_SIZE(x) 	(SDATA_INDEX_SIZE(SVEC_SDATAPTR(x)))
#define SVEC_INDEX_PTR(x) 	(SDATA_INDEX_PTR(SVEC_SDATAPTR(x)))

/** @return True if input is a scalar */
#define IS_SCALAR(x)	(((x)->dimension) < 0 ? 1 : 0 )

/** @return True if input is a NULL, represented internally as a NVP */
#define IS_NVP(x)  (memcmp(&(x),&(NVP),sizeof(double)) == 0)

static inline int check_scalar(int i1, int i2)
{
	if ((!i1) && (!i2)) return(0);
	else if (i1 && i2) return(3);
	else if (i1)  return(1);
	else if (i2) return(2);
	return(0);
}

/*
 * This routine supplies a pointer to a SparseData derived from an SvecType.
 * The SvecType is a serialized structure with fixed memory allocations, so
 * care must be taken not to append to the embedded StringInfo structs
 * without re-serializing the SparseData into the SvecType.
 */
static inline SparseData sdata_from_svec(SvecType *svec)
{
	char *sdataptr   = SVEC_SDATAPTR(svec);
	SparseData sdata = (SparseData)sdataptr;
	sdata->vals  = (StringInfo)SDATA_DATA_SINFO(sdataptr);
	sdata->index = (StringInfo)SDATA_INDEX_SINFO(sdataptr);
	sdata->vals->data   = SVEC_VALS_PTR(svec);
	if (sdata->index->maxlen == 0)
	{
		sdata->index->data = NULL;
	} else
	{
		sdata->index->data  = SVEC_INDEX_PTR(svec);
	}
	return(sdata);
}

static inline void printout_svec(SvecType *svec, char *msg, int stop);
static inline void printout_svec(SvecType *svec, char *msg, int stop)
{
	printout_sdata((SparseData)SVEC_SDATAPTR(svec), msg, stop);
	elog(NOTICE,"len,dimension=%d,%d",VARSIZE(svec),svec->dimension);
}

char *svec_out_internal(SvecType *svec);
SvecType *svec_from_sparsedata(SparseData sdata,bool trim);
ArrayType *svec_return_array_internal(SvecType *svec);
char *svec_out_internal(SvecType *svec);
SvecType *svec_make_scalar(float8 value);
SvecType *svec_from_float8arr(float8 *array, int dimension);
SvecType *op_svec_by_svec_internal(enum operation_t operation, SvecType *svec1, SvecType *svec2);
SvecType *svec_operate_on_sdata_pair(int scalar_args,enum operation_t operation,SparseData left,SparseData right);
SvecType *makeEmptySvec(int allocation);
SvecType *reallocSvec(SvecType *source);

Datum svec_in(PG_FUNCTION_ARGS);
Datum svec_out(PG_FUNCTION_ARGS);
Datum svec_return_vector(PG_FUNCTION_ARGS);
Datum svec_return_array(PG_FUNCTION_ARGS);
Datum svec_send(PG_FUNCTION_ARGS);
Datum svec_recv(PG_FUNCTION_ARGS);

// Operators
Datum svec_pow(PG_FUNCTION_ARGS);
Datum svec_equals(PG_FUNCTION_ARGS);
Datum svec_minus(PG_FUNCTION_ARGS);
Datum svec_plus(PG_FUNCTION_ARGS);
Datum svec_div(PG_FUNCTION_ARGS);
Datum svec_dot(PG_FUNCTION_ARGS);
Datum svec_l2norm(PG_FUNCTION_ARGS);
Datum svec_count(PG_FUNCTION_ARGS);
Datum svec_mult(PG_FUNCTION_ARGS);
Datum svec_log(PG_FUNCTION_ARGS);
Datum svec_l1norm(PG_FUNCTION_ARGS);
Datum svec_summate(PG_FUNCTION_ARGS);

Datum float8arr_minus_float8arr(PG_FUNCTION_ARGS);
Datum svec_minus_float8arr(PG_FUNCTION_ARGS);
Datum float8arr_minus_svec(PG_FUNCTION_ARGS);
Datum float8arr_plus_float8arr(PG_FUNCTION_ARGS);
Datum svec_plus_float8arr(PG_FUNCTION_ARGS);
Datum float8arr_plus_svec(PG_FUNCTION_ARGS);
Datum float8arr_mult_float8arr(PG_FUNCTION_ARGS);
Datum svec_mult_float8arr(PG_FUNCTION_ARGS);
Datum float8arr_mult_svec(PG_FUNCTION_ARGS);
Datum float8arr_div_float8arr(PG_FUNCTION_ARGS);
Datum svec_div_float8arr(PG_FUNCTION_ARGS);
Datum float8arr_div_svec(PG_FUNCTION_ARGS);
Datum svec_dot_float8arr(PG_FUNCTION_ARGS);
Datum float8arr_dot_svec(PG_FUNCTION_ARGS);

// Casts
Datum svec_cast_int2(PG_FUNCTION_ARGS);
Datum svec_cast_int4(PG_FUNCTION_ARGS);
Datum svec_cast_int8(PG_FUNCTION_ARGS);
Datum svec_cast_float4(PG_FUNCTION_ARGS);
Datum svec_cast_float8(PG_FUNCTION_ARGS);
Datum svec_cast_numeric(PG_FUNCTION_ARGS);

Datum float8arr_cast_int2(PG_FUNCTION_ARGS);
Datum float8arr_cast_int4(PG_FUNCTION_ARGS);
Datum float8arr_cast_int8(PG_FUNCTION_ARGS);
Datum float8arr_cast_float4(PG_FUNCTION_ARGS);
Datum float8arr_cast_float8(PG_FUNCTION_ARGS);
Datum float8arr_cast_numeric(PG_FUNCTION_ARGS);

Datum svec_cast_float8arr(PG_FUNCTION_ARGS);
Datum svec_unnest(PG_FUNCTION_ARGS);
Datum svec_pivot(PG_FUNCTION_ARGS);

#endif  /* SPARSEVECTOR_H */
