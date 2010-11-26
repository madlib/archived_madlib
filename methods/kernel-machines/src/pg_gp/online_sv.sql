-- This module implements the class of online learning with kernels algorithms described in
--  Jyrki Kivinen, Alexander J. Smola and Robert C. Williamson, Online Learning with Kernels,
--  IEEE Transactions on Signal Processing, 52(8), 2165-2176, 2004.
--
-- The implementation follows the original description in the Kivinen et al paper faithfully,
-- except that we only update the support vector model when we make a significant error.  
-- The original algorithms update the support vector model at every step, even when no error 
-- was made, in the name of regularisation. For practical purposes, and this is verified 
-- empirically to a certain degree, I think updating only when necessary is both faster and 
-- better from a learning point of view.
--
-- Usage:
-- create table train_data ( id int, ind float8[], label float8 ) distributed by (id);
-- create table results ( id text, model model_rec ) distributed by (id);
-- create table sv_model ( weight float8, sv float8[] ) distributed by (weight);
-- select generateData(1000, 5);
-- insert into results (select 'myexp', online_sv_reg_agg(ind, label) from train_data);
--  or
-- insert into results (select 'myexp' || gp_segment_id, online_sv_reg_agg(ind, label) from train_data group by gp_segment_id);
-- select storeModel('myexp');
-- select svs_predict('{1,2,10}');


-- The following is the structure to record the results of a learning process.
-- We work with arrays of float8 for now; we'll extend the code to work with sparse vectors next.
DROP TYPE IF EXISTS model_rec CASCADE;
CREATE TYPE model_rec AS (
       inds int,        -- number of individuals processed 
       cum_err float8,  -- cumulative error
       epsilon float8,  -- the size of the epsilon tube around the hyperplane, adaptively adjusted by algorithm
       nsvs int,        -- number of support vectors
       ind_dim int,     -- the dimension of the individuals
       weights float8[],       -- the weight of the support vectors
       individuals float8[][]  -- the array of support vectors
);

-- Kernel functions are a generalisation of inner products. 
-- They provide the means by which we can extend linear machines to work in non-linear transformed feature spaces.
-- Here we specify the dot product as the kernel; it can be replace with any other kernel, including the polynomial
-- and Gaussian kernels defined below.
CREATE OR REPLACE FUNCTION kernel(x float8[][], idx int, y float8[]) RETURNS float8 AS $$
DECLARE
	len INT;
	ind float8[];
BEGIN
	len := array_upper(y,1);
	FOR i IN 1..len LOOP
	    ind[i] := x[idx][i];
	END LOOP;
	RETURN dot_kernel(ind, y);
END
$$ LANGUAGE plpgsql;

-- This is just inner product. For efficiency, this can be implemented as a C UDF. In fact, if the sparse vector library
-- is installed, one can just define the body of the function to be RETURN dot(x,y);.
CREATE OR REPLACE FUNCTION dot_kernel(x float8[], y float8[]) RETURNS float8 AS $$
DECLARE 
	len int;
	ret float8 := 0;
BEGIN
	len := array_upper(y,1);
	FOR i in 1..len LOOP
	    ret := ret + x[i]*y[i];
	END LOOP;
	RETURN ret;
END;
$$ LANGUAGE plpgsql;

-- Here are two other standard kernels.
CREATE OR REPLACE FUNCTION polynomial_kernel(x float8[], y float8[], degree int) RETURNS float8 AS $$
BEGIN
	RETURN dot_kernel(x,y) ^ degree;
END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION gaussian_kernel(x float8[], y float8[], gamma float) RETURNS float8 AS $$
BEGIN
	RETURN exp(-1.0 * gamma * dot_kernel(x-y,x-y));
END;
$$ LANGUAGE plpgsql;

-- This implements the prediction function: f(x) = sum_i weight[i] * kernel(suppor_vector[i], x).
CREATE OR REPLACE FUNCTION
svs_predict(svs model_rec, ind float8[]) 
RETURNS float8 AS $$
DECLARE
	ret FLOAT8 := 0;
BEGIN
	FOR i IN 1..svs.nsvs LOOP
	    ret := ret + svs.weights[i] * kernel(svs.individuals, i, ind);
        END LOOP;
	RETURN ret;
END;
$$ LANGUAGE plpgsql;

-- This is the main online support vector regression learning algorithm. 
-- The function updates the support vector model as it processes each new training example.
-- This function is wrapped in an aggregate function to process all the training examples stored in a table.  
-- The learning parameters (blambda, slambda, and nu) are hardcoded at the moment. 
-- We may want to make them input parameters at some stage, although the naive user would probably be daunted with the prospect
-- of having to specify them. 
CREATE OR REPLACE FUNCTION online_sv_reg_update(svs model_rec, ind float8[], label float8) 
RETURNS model_rec AS $$
DECLARE
	blambda FLOAT8 := 0.05; -- learning rate
	slambda FLOAT8 := 0.2;  -- regularisation parameter
	nu FLOAT8 := 0.001;     -- compression parameter, a number between 0 and 1; the fraction of the training data that appear as support vectors
	p FLOAT8;       -- prediction for the input individual
	diff FLOAT8;    -- difference between p and label
	error FLOAT8;   -- absolute value of diff
	weight FLOAT8;  -- the weight of ind if it turns out to be a support vector
BEGIN
	IF svs IS NULL THEN
	    svs := (0, 0, 0, 1, array_upper(ind,1), '{0}', array[ind]);  -- we have to be careful to initialise a multi-dimensional array
        END IF;

	p := svs_predict(svs, ind);
	diff := label - p;
	error := abs(diff);
	svs.inds := svs.inds + 1;
	svs.cum_err := svs.cum_err + error;

	IF (error > svs.epsilon) THEN
	    FOR i IN 1..svs.nsvs LOOP -- Unlike the original algorithm, this rescaling is only done when we make a large enough error.
	    	 svs.weights[i] := svs.weights[i] * (1 - blambda * slambda);
            END LOOP;

	    weight := blambda;
	    IF (diff < 0) THEN weight := -1 * weight; END IF;
	    svs.nsvs := svs.nsvs + 1;
	    svs.weights[svs.nsvs] := weight;
	    svs.individuals := array_cat(svs.individuals, ind);
	    svs.epsilon := svs.epsilon + (1 - nu) * blambda;      
	ELSE
	    svs.epsilon := svs.epsilon - blambda * nu;
        END IF;

	return svs;
END
$$ LANGUAGE plpgsql;

DROP AGGREGATE online_sv_reg_agg(float8[], float8);
CREATE AGGREGATE online_sv_reg_agg(float8[], float8) (
       sfunc = online_sv_reg_update,
       stype = model_rec
);

-- CREATE OR REPLACE FUNCTION transform_rec(svs model_rec) RETURNS SETOF sv_model AS $$
-- DECLARE
-- 	sv sv_model;
-- BEGIN
--         FOR i IN 1..svs.nsvs LOOP 
--       	 sv.weight = svs.weights[i];
--	         FOR j IN 1..svs.ind_dim LOOP sv.sv[j] = svs.individuals[i][j]; END LOOP;
-- 	         RETURN NEXT sv;
--     	   END LOOP;
-- END;
-- $$ LANGUAGE plpgsql;

-- This function transforms a model_rec into a set of (weight, support_vector) values for the purpose of storage in a table.
CREATE OR REPLACE FUNCTION transform_rec(ind_dim int, weights float8[], individuals float8[][]) RETURNS SETOF sv_model AS $$
DECLARE
	nsvs INT;
	sv sv_model;
BEGIN
	nsvs = array_upper(weights,1);
	FOR i IN 1..nsvs LOOP 
       	    sv.weight = weights[i];
	    FOR j IN 1..ind_dim LOOP sv.sv[j] = individuals[i][j]; END LOOP; -- we copy the individual because we can't say sv.sv[j] = individuals[i]
	    RETURN NEXT sv;
     	END LOOP;
END;
$$ LANGUAGE plpgsql;

-- This function stores a model_rec stored with modelname in the results table into the sv_model table.
CREATE OR REPLACE FUNCTION storeModel(modelname TEXT) RETURNS VOID AS $$
DECLARE
	myind_dim INT;
	myweights float8[];
	myindividuals float8[][];
--	mysvs model_rec;
BEGIN
--	SELECT INTO mysvs model FROM results WHERE id = modelname; -- for some strange reason this line doesn't work....
	SELECT INTO myind_dim (model).ind_dim FROM results WHERE id = modelname;
	SELECT INTO myweights (model).weights FROM results WHERE id = modelname;
	SELECT INTO myindividuals (model).individuals FROM results WHERE id = modelname;
	DELETE FROM sv_model;
 	INSERT INTO sv_model (SELECT * FROM transform_rec(myind_dim, myweights, myindividuals));
END;
$$ LANGUAGE plpgsql;

-- This function performs prediction using a support vector machine stored in the sv_model table.
CREATE OR REPLACE FUNCTION svs_predict(ind float8[], OUT ret float8) RETURNS FLOAT8 AS $$
BEGIN
	SELECT INTO ret sum(weight * kernel(sv, ind)) FROM sv_model;
END;
$$ LANGUAGE plpgsql;

-- Generate artificial training data 
CREATE OR REPLACE FUNCTION randomInd(d INT) RETURNS float8[] AS $$
DECLARE
    ret float8[];
BEGIN
    FOR i IN 1..(d-1) LOOP
        ret[i] = RANDOM() * 40 - 20;
    END LOOP;
    IF (RANDOM() > 0.5) THEN
        ret[d] = 10;
    ELSE 
        ret[d] = -10;
    END IF;
    RETURN ret;
END
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION generateData(num int, dim int) RETURNS VOID AS $$
    plpy.execute("DELETE FROM train_data")
    plpy.execute("INSERT INTO train_data SELECT a.val, randomInd(" + str(dim) + "), 0 FROM (SELECT generate_series(1," + str(num) + ") AS val) AS a")
    plpy.execute("UPDATE train_data SET label = targetFunc(ind)")
$$ LANGUAGE 'plpythonu';

CREATE OR REPLACE FUNCTION targetFunc(ind float8[]) RETURNS float8 AS $$
DECLARE
    dim int;
BEGIN
    dim = array_upper(ind,1);
    IF (ind[dim] = 10) THEN RETURN 50; END IF;
    RETURN -50;
END
$$ LANGUAGE plpgsql;






