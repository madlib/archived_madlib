SET client_min_messages = 'ERROR';

DROP function IF EXISTS madlib.install_test();
CREATE FUNCTION madlib.install_test() RETURNS TEXT AS $$ 
declare
	an float[] := '{1,2,3}';
	b float[] := '{4,5,7}';
	result_num1 float := 0;
	result_num2 float := 0;
	result_num3 float := 0;
	result TEXT;
begin

	SELECT INTO result_num1 madlib.array_dot(madlib.array_mult(madlib.array_add(an,b), madlib.array_sub(an,b)),madlib.array_div(an,b));
	b[4] = NULL;
	SELECT INTO result_num2 (madlib.array_max(b)+madlib.array_min(b)+madlib.array_sum(b)+madlib.array_sum_big(b)+
	madlib.array_mean(b)+madlib.array_stddev(b));
	SELECT INTO result_num3 madlib.array_sum(madlib.array_scalar_mult(madlib.array_fill(madlib.array_of_float(20), 234.343::FLOAT8),3.7::FLOAT));
	result_num1 = result_num1+result_num2+result_num3-17361.6696953194;
	
	SELECT INTO result CASE WHEN((result_num1 < .01)AND(result_num1 > -.01)) THEN 'PASS' ELSE 'FAILED' END;
	RETURN result;
end $$ language plpgsql;

SELECT madlib.install_test();
DROP FUNCTION madlib.install_test();