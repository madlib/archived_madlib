-- Example usage for regression:
delete from sv_results;
delete from sv_model;

select generateRegData(1000, 5);
insert into sv_results (select 'myexp', online_sv_reg_agg(ind, label) from sv_train_data);
select storeModel('myexp');
select svs_predict('myexp', '{1,2,4,20,10}');
   
-- To learn multiple support vector regression models
insert into sv_results (select 'myexp' || gp_segment_id, online_sv_reg_agg(ind, label) from sv_train_data group by gp_segment_id);
select storeModel('myexp', 2); -- n is the number of segments FIX ME
select * from svs_predict_combo('myexp', 2, '{1,2,4,20,10}');

-- Example usage for classification:
select generateClData(2000, 5);
insert into sv_results (select 'myexpc', online_sv_cl_agg(ind, label) from sv_train_data);
select storeModel('myexpc');
select svs_predict('myexpc', '{10,-2,4,20,10}');
   
-- To learn multiple support vector models, replace the above by 
insert into sv_results (select 'myexpc' || gp_segment_id, online_sv_cl_agg(ind, label) from sv_train_data group by gp_segment_id);
select storeModel('myexpc', 2); -- n is the number of segments FIX ME
select * from svs_predict_combo('myexpc', 2, '{10,-2,4,20,10}');

-- Example usage for novelty detection:
select generateNdData(100, 2);
insert into sv_results (select 'myexpnd', online_sv_nd_agg(ind) from sv_train_data);
select storeModel('myexpnd');
select svs_predict('myexpnd', '{10,-10}');  
select svs_predict('myexpnd', '{-1,-1}');  