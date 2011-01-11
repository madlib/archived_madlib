/*
	This code computes quantile value from a table;
	table_name - is the name of the table from which quantile is to be taken
	col_name - is the name of the column that is to be used for quantile culculation
	quantile - is the qunatile value desired \in (0,1)
	
	Example:
		SELECT quantile(MyTaxEvasionRecords, AnountUnderpaid, .3);
*/

DROP FUNCTION IF EXISTS quantile(table_name TEXT, col_name TEXT, quantile FLOAT);CREATE OR REPLACE FUNCTION quantile( table_name TEXT, col_name TEXT, quantile FLOAT) RETURNS FLOAT AS $$declare  size FLOAT[];  curr FLOAT;BeginEXECUTE	'SELECT ARRAY[COUNT(*), MIN( ' || col_name || ' ), MAX(' || col_name || ' ), AVG( ' || col_name || ' )] FROM ' || table_name || ';' INTO size;	size[1] = size[1]*quantile;    LOOP    	EXECUTE 'SELECT COUNT(*) FROM '|| table_name || ' WHERE ' || col_name  || ' <= ' || size[4] || ';'  INTO curr;
    	RAISE INFO 'VALUES NOW: % WANT: % FRACT: % MIN: % MAX: % CURR: %', curr, size[1], curr/size[1] + size[1]/curr, size[2], size[3], size[4];    	IF((curr - size[1]) > 1) THEN    		size[3] = size[4];    		size[4] = (size[2]+size[4])/2.0;    	ELSIF((size[1] - curr) > 1) THEN    		size[2] = size[4];    		size[4] = (size[3]+size[4])/2.0;    	ELSE    		RETURN size[4];    	END IF;    END LOOP;    RETURN size[4];end$$ LANGUAGE plpgsql;