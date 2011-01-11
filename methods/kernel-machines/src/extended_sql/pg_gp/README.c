/*! \defgroup kernel-machines

\par About

    Support vector machines (SVMs) and related kernel methods have been one of 
    the most popular and well-studied machine learning techniques of the 
    past 15 years, with an amazing number of innovations and applications.

    In a nutshell, an SVM model f(x) takes the form of
\code
        f(x) = sum_i alpha_i k(x_i,x),
\endcode
    where each alpha_i is a real number, each x_i is a data point from the
    training set (called a support vector), and k(.,.) is a kernel function 
    that measures how `similar' two objects are. In regression, f(x) is the 
    regression function we seek. In classification, f(x) serves as the 
    decision boundary; so for example in binary classification, the predictor 
    can output class 1 for object x if f(x) >= 0, and class 2 otherwise.

    In the case when the kernel function k(.,.) is the standard inner product 
    on vectors, f(x) is just an alternative way of writing a linear function
\code
        f'(x) = <w,x>,
\endcode
    where w is a weight vector having the same dimension as x. One of the 
    key points of SVMs is that we can use more fancy kernel functions to 
    efficiently learn linear models in high-dimensional feature spaces, 
    since k(x_i,x_j) can be understood as an inner product in the feature
    space:
\code
        k(x_i,x_j) = <phi(x_i),phi(x_j)>,
\endcode
    where phi(x) projects x into a (possibly infinite-dimensional) feature
    space.

    There are many algorithms for learning kernel machines. This module 
    implements the class of online learning with kernels algorithms 
    described in \n

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


\par Prerequisites

    - None at this point. Will need to install the Greenplum sparse vector 
      SVEC datatype eventually.

\par Usage/API

   - Preparation of the input
       Insert the training data into the table sv_train_data, which has
       the following structure:
\code    
        (       id    INT,       -- point ID
                ind   FLOAT8[],  -- data point
                label FLOAT8     -- label of data point
    	)
\endcode    
        Note: The label field is not required for novelty detection.
    
   - The main learning functions
     -  Online regression learning is achieved through the following aggregate
        function
        \code
	MADLIB_SCHEMA.online_sv_reg_agg(x float8[], y float8),
        \endcode
	where x is a data point and y its regression value. The function
	returns a model_rec data type. (More on that later.) 
     -  Online classification learning is achieved through the following
        aggregate function
        \code
        MADLIB_SCHEMA.online_sv_cl_agg(x float8[], y float8),
        \endcode
        where x is a data point and y its class (usually +1 or -1). The 
	function returns a model_rec data type.
     -  Online novelty detection is achieved through the following 
        aggregate function
        \code
        MADLIB_SCHEMA.online_sv_nd_agg(x float8[]),
        \endcode
        where x is a data point. Note that novelty detection is an unsupervised
        learning technique, so no label is required. The function returns a
        model_rec data type.
     In each case, learning is done by running the corresponding aggregate 
     function through a training set stored in a table/view.

     The model_rec data type contains the following two fields
\code
        weights     FLOAT8[],   -- the weight of the support vectors
        individuals FLOAT8[][]  -- the array of support vectors
\endcode
     as well as other information obtained through the learning process
     like cumulative error, margin achieved, etc.

  - Subsidiary functions
     - We can unnest the support vectors in a model_rec and store them in
       the sv_model table using the function
       \code
       MADLIB_SCHEMA.storeModel(model_name text),
       \endcode 
       where model_name is the name of the model stored temporarily in the
       sv_results table. (FIX ME: Make storeModel() take a model_rec as 
       input.)
     - Having stored a model, we can use the following function to make
       predictions on new test data points:
       \code
       MADLIB_SCHEMA.svs_predict(model_name text, x float8[]),
       \endcode
       where model_name is the name of the model stored and x is a data point.

\par Examples

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


\par To Do

    - Add support for sparse vectors (now it's only array of float8s).

*/


    
