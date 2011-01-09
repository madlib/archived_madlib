/*! \defgroup kernel-machines

\par About

    This module implements the class of online learning with kernels 
    algorithms described in \n

      \t Jyrki Kivinen, Alexander J. Smola and Robert C. Williamson, \n
      \t Online Learning with Kernels, IEEE Transactions on Signal Processing, 52(8), 2165-2176, 2004.\n

    See also the book \n
  
      \t Bernhard Scholkopf and Alexander J. Smola, Learning with Kernels: \n
      \t Support Vector Machines, Regularization, Optimization, and Beyond, 
      MIT Press, 2002 \n
 
    for much more details.

    The implementation follows the original description in the Kivinen et al 
    paper faithfully, except that we only update the support vector model 
    when we make a significant error. The original algorithms update the 
    support vector model at every step, even when no error was made, in the 
    name of regularisation. For practical purposes, and this is verified 
    empirically to a certain degree, updating only when necessary is both 
    faster and better from a learning-theoretic point of view, at least in 
    the i.i.d. setting.

    Methods for classification, regression and novelty detection are 
    available. Multiple instances of the algorithms can be executed 
    in parallel on different subsets of the training data. The resultant
    support vector models can then be combined using standard techniques
    like averaging or majority voting.

    Training data points are accessed via a table or a view. The support
    vector models can also be stored in tables for fast execution.

\par To Do

    - Add support for sparse vectors (now it's only array of float8s).

\par Prerequisites

    - None at this point. Will need to install the Greenplum sparse vector 
    SVEC datatype eventually.


\par Installation

    -# Create database objects:
\code    
        psql -f online_sv.sql -d <database>
\endcode

\par Preparation of the Input

    - Insert the training data into the table sv_train_data, which has
       the following structure:
\code    
        (       id 		INT,  	    -- point ID
                ind		FLOAT8[],   -- data point
                label		FLOAT8	    -- label of data point
    	)
\endcode    
        Note: The label field is not required for novelty detection.
    

\par Execution (in-database)

   - Example usage for regression:
\code
       testdb=# select MADLIB_SCHEMA.generateRegData(1000, 5);
       testdb=# insert into MADLIB_SCHEMA.sv_results (select 'myexp', MADLIB_SCHEMA.online_sv_reg_agg(ind, label) from MADLIB_SCHEMA.sv_train_data);
       testdb=# select MADLIB_SCHEMA.storeModel('myexp');
       testdb=# select MADLIB_SCHEMA.svs_predict('myexp', '{1,2,4,20,10}');
\endcode
     To learn multiple support vector models, replace the above by 
\code
       testdb=# insert into MADLIB_SCHEMA.sv_results 
                   (select 'myexp' || gp_segment_id, MADLIB_SCHEMA.online_sv_reg_agg(ind, label) from MADLIB_SCHEMA.sv_train_data group by gp_segment_id);
       testdb=# select MADLIB_SCHEMA.storeModel('myexp', n); -- n is the number of segments
       testdb=# select * from MADLIB_SCHEMA.svs_predict_combo('myexp', n, '{1,2,4,20,10}');
\endcode

   - Example usage for classification:
\code 
       testdb=# select MADLIB_SCHEMA.generateClData(2000, 5);
       testdb=# insert into MADLIB_SCHEMA.sv_results (select 'myexpc', MADLIB_SCHEMA.online_sv_cl_agg(ind, label) from MADLIB_SCHEMA.sv_train_data);
       testdb=# select MADLIB_SCHEMA.storeModel('myexpc');
       testdb=# select MADLIB_SCHEMA.svs_predict('myexpc', '{10,-2,4,20,10}');
\endcode   
     To learn multiple support vector models, replace the above by 
\code
       testdb=# insert into MADLIB_SCHEMA.sv_results 
                   (select 'myexpc' || gp_segment_id, MADLIB_SCHEMA.online_sv_cl_agg(ind, label) from MADLIB_SCHEMA.sv_train_data group by gp_segment_id);
       testdb=# select MADLIB_SCHEMA.storeModel('myexpc', n); -- n is the number of segments
       testdb=# select * from MADLIB_SCHEMA.svs_predict_combo('myexpc', n, '{10,-2,4,20,10}');
\endcode

   - Example usage for novelty detection:
\code
       testdb=# select MADLIB_SCHEMA.generateNdData(100, 2);
       testdb=# insert into MADLIB_SCHEMA.sv_results (select 'myexpnd', MADLIB_SCHEMA.online_sv_nd_agg(ind) from MADLIB_SCHEMA.sv_train_data);
       testdb=# select MADLIB_SCHEMA.storeModel('myexpnd');
       testdb=# select MADLIB_SCHEMA.svs_predict('myexpnd', '{10,-10}');  
       testdb=# select MADLIB_SCHEMA.svs_predict('myexpnd', '{-1,-1}');  
\endcode

*/


    
