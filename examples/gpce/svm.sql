\qecho =================================================================
\qecho === Running: support vector machines ============================
\qecho =================================================================
\qecho 

TRUNCATE TABLE madlib.sv_model;
TRUNCATE TABLE madlib.sv_results;
TRUNCATE TABLE madlib.sv_train_data;

\qecho =================================================================
\qecho === SVM: regression =========================================
\qecho =================================================================
\qecho 

\qecho === generate 500 5-dimensional data ============================
select madlib.generateRegData(500, 5);

\qecho === create a model ==============================================
select madlib.sv_regression('madlib.sv_train_data', 'myexp', false);

\qecho === predict labels of some new data points ======================
select madlib.svs_predict('myexp', '{1,2,4,20,10}');
select madlib.svs_predict('myexp', '{1,2,4,20,-10}');

\qecho === create multiple models ======================================
select madlib.sv_regression('madlib.sv_train_data', 'myexp', true);
select * from madlib.svs_predict_combo('myexp', '{1,2,4,20,10}');

\qecho =================================================================
\qecho === SVM: classification =====================================
\qecho =================================================================
\qecho

\qecho === generate 500 5-dimensional data ============================
select madlib.generateClData(500, 5);

\qecho === create a model ==============================================
select madlib.sv_classification('madlib.sv_train_data', 'myexpc', false);

\qecho === predict labels of some new data points ======================
select madlib.svs_predict('myexpc', '{10,-2,4,20,10}');

\qecho === create multiple models ======================================
select madlib.sv_classification('madlib.sv_train_data', 'myexpc', true);
select * from madlib.svs_predict_combo('myexpc', '{10,-2,4,20,10}');

\qecho =================================================================
\qecho === SVM: novelty detection ==================================
\qecho =================================================================
\qecho

\qecho === generate 100 2-dimensional data ============================
select madlib.generateClData(100, 2);

\qecho === learn and predict for a single model ========================
select madlib.sv_novelty_detection('madlib.sv_train_data', 'myexpnd', false);
select madlib.svs_predict('myexpnd', '{10,-10}');  
select madlib.svs_predict('myexpnd', '{-1,-1}');

\qecho === learn and predict for multiple models =======================
select madlib.sv_novelty_detection('madlib.sv_train_data', 'myexpnd', true);
select * from madlib.svs_predict_combo('myexpnd', '{10,-10}');  
select * from madlib.svs_predict_combo('myexpnd', '{-1,-1}');  
