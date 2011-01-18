/*! \defgroup kernel-machines Support Vector Machines

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
    since k(x_i,x_j) can be understood as an efficient way of computing an 
    inner product in the feature space:
\code
        k(x_i,x_j) = <phi(x_i),phi(x_j)>,
\endcode
    where phi(x) projects x into a (possibly infinite-dimensional) feature
    space.

    There are many algorithms for learning kernel machines. This module 
    implements the class of online learning with kernels algorithms 
    described in \n

      - Jyrki Kivinen, Alexander J. Smola and Robert C. Williamson, \n
        Online Learning with Kernels, IEEE Transactions on Signal Processing, 52(8), 2165-2176, 2004.\n

    See also the book \n
  
      - Bernhard Scholkopf and Alexander J. Smola, Learning with Kernels: \n
        Support Vector Machines, Regularization, Optimization, and Beyond, 
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

    - None at this point. Will need the Greenplum sparse vector 
      SVEC datatype eventually.

\par Usage/API

  Here are the main learning functions.

     -  Regression learning is achieved through the following function
        \code
        madlib.sv_regression(input_table text, model_name text, parallel bool)
        \endcode

     -  Classification learning is achieved through the following function
        \code
        madlib.sv_classification(input_table text, model_name text, parallel bool)
        \endcode

     -  Novelty detection is achieved through the following function
        \code
        madlib.sv_novelty_detection(input_table text, model_name text, parallel bool)
        \endcode

  In each case, input_table is the name of the table/view with the training 
  data, model_name is the name under which we want to store the resultant 
  learned model, and parallel is a flag indicating whether the system
  should learn multiple models in parallel. (The multiple models can be
  combined to make predictions; more on that shortly.)

  Here are the functions that can be used to make predictions on new
  data points.

     - To make predictions on new data points using a single model
       learned previously, we use the function
       \code
       madlib.svs_predict(model_name text, x float8[]),
       \endcode
       where model_name is the name of the model stored and x is a data point.

     - To make predictions on new data points using multiple models
       learned in parallel, we use the function
       \code
       madlib.svs_predict_combo(model_name text, x float8[]),
       \endcode
       where model_name is the name under which the models are stored, and x 
       is a data point.

  Models that have been stored can be deleted using the function
  \code
       madlib.drop_sv_model(modelname text).
  \endcode

\par Examples

As a general first step, we need to prepare and populate an input 
table/view with the following structure:
\code   
        CREATE TABLE my_schema.my_input_table 
        (       
                id    INT,       -- point ID
                ind   FLOAT8[],  -- data point
                label FLOAT8     -- label of data point
    	);
\endcode    
     Note: The label field is not required for novelty detection.
    

Example usage for regression:
     -# We can randomly generate 1000 5-dimensional data labelled by the simple target function 
        \code
        t(x) = if x[5] = 10 then 50 else if x[5] = -10 then 50 else 0;
        \endcode
        and store that in the madlib.sv_train_data table as follows:
        \code
        testdb=# select madlib.generateRegData(1000, 5);
        \endcode
     -# We can now learn a regression model and store the resultant model
        under the name  'myexp'.
        \code
        testdb=# select madlib.sv_regression('madlib.sv_train_data', 'myexp', false);
        \endcode
     -# We can now start using it to predict the labels of new data points 
        like as follows:
        \code
        testdb=# select madlib.svs_predict('myexp', '{1,2,4,20,10}');
        testdb=# select madlib.svs_predict('myexp', '{1,2,4,20,-10}');
        \endcode
     -# To learn multiple support vector models, we replace the learning step above by 
        \code
        testdb=# select madlib.sv_regression('madlib.sv_train_data', 'myexp', true);
        \endcode
       The resultant models can be used for prediction as follows:
       \code
       testdb=# select * from madlib.svs_predict_combo('myexp', '{1,2,4,20,10}');
       \endcode

Example usage for classification:
     -# We can randomly generate 2000 5-dimensional data labelled by the simple
        target function 
        \code
        t(x) = if x[1] > 0 and  x[2] < 0 then 1 else -1;
        \endcode
        and store that in the madlib.sv_train_data table as follows:
        \code 
        testdb=# select madlib.generateClData(2000, 5);
        \endcode
     -# We can now learn a classification model and store the resultant model
        under the name  'myexpc'.
        \code
        testdb=# select madlib.sv_classification('madlib.sv_train_data', 'myexpc', false);
        \endcode
     -# We can now start using it to predict the labels of new data points 
        like as follows:
        \code
        testdb=# select madlib.svs_predict('myexpc', '{10,-2,4,20,10}');
        \endcode 
     -# To learn multiple support vector models, replace the model-building and prediction steps above by 
        \code
        testdb=# select madlib.sv_classification('madlib.sv_train_data', 'myexpc', true);
        testdb=# select * from madlib.svs_predict_combo('myexpc', '{10,-2,4,20,10}');
        \endcode

Example usage for novelty detection:
     -# We can randomly generate 100 2-dimensional data (the normal cases)
        and store that in the madlib.sv_train_data table as follows:
        \code
        testdb=# select madlib.generateNdData(100, 2);
        \endcode
     -# Learning and predicting using a single novelty detection model can be done as follows:
        \code
        testdb=# select madlib.sv_novelty_detection('madlib.sv_train_data', 'myexpnd', false);
        testdb=# select madlib.svs_predict('myexpnd', '{10,-10}');  
        testdb=# select madlib.svs_predict('myexpnd', '{-1,-1}');  
        \endcode
     -# Learning and predicting using multiple models can be done as follows:
        \code
        testdb=# select madlib.sv_novelty_detection('madlib.sv_train_data', 'myexpnd', true);
        testdb=# select * from madlib.svs_predict_combo('myexpnd', '{10,-10}');  
        testdb=# select * from madlib.svs_predict_combo('myexpnd', '{-1,-1}');  
        \endcode

\par To Do

    - Add support for sparse vectors (now it's only array of float8s).

*/


    
