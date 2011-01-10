#!/usr/bin/env python

"""@namespace kmeans

Implementation of k-means clustering algorithm (currently works on SVEC data type only).

\par About:

This module implements "k-means clustering" method for a set of data points
accessed via a table or a view. Initial centroids are found according to  
k-means++ algorithm (http://en.wikipedia.org/wiki/K-means%2B%2B). 
Further adjustments are based either on a full data set or on a sample of 
points, such that there are at least 200 points from each cluster.


\par Prerequisites:

Sparse vector data type (svec).


\par Example:

1) Prepare the input table/view with the following structure:

@verbatim(
	pid 		INT,  	-- point ID
	position 	SVEC	-- point coordinates
)@endverbatim

Note!:  You can use "create_input.sql" script to create an input table 
        and populate it with random data. See the script for details. 
        By default it will create a table called madlib.kmeans_input
        with 10,000 points (1,000 dimension each).
    

2) Execute the function (in-database): (in-database):

Call the kmeans_run(...) stored procedure, e.g.:  

@verbatim
SQL> psql -c "select madlib.kmeans_run( 'madlib.kmeans_input', 10, 1, 'run1', 'test');"@endverbatim
    
Parameters:
    - input_table:      table with the source data
    - k:                max number of clusters to consider 
                        (must be smaller than number of input points)
    - goodness:         goodness of fit test flag [0,1]
    - run_id:           execution identifier
    - output_schema:    target schema for output tables (points, centroids)
        

2) Execute the function (in-database): (outside-database):

Call the provided sample script "kmeans_test.py" with the database connection details:

@verbatim
kmeans_test.py -h <host> -p <port> -U <user> -d <database> @endverbatim
    

\par Sample Output:

@verbatim
INFO: 2010-11-19 11:10:23 : Parameters:
INFO: 2010-11-19 11:10:23 :  * k = 10 (number of centroids)
INFO: 2010-11-19 11:10:23 :  * input_table = madlib.kmeans_input
INFO: 2010-11-19 11:10:23 :  * goodness = 1 (GOF test on)
INFO: 2010-11-19 11:10:23 :  * run_id = testrun
INFO: 2010-11-19 11:10:23 :  * output_schema = madlib
INFO: 2010-11-19 11:10:23 : Seeding 10 centroids...
INFO: 2010-11-19 11:10:24 : Using sample data set for analysis... (9200 out of 10000 points)
INFO: 2010-11-19 11:10:24 : Iteration 1...
INFO: 2010-11-19 11:10:25 : Iteration 2...
INFO: 2010-11-19 11:10:26 : Exit reason: fraction of reassigned nodes is smaller than the limit: 0.001
INFO: 2010-11-19 11:10:26 : Expanding cluster assignment to all points...
INFO: 2010-11-19 11:10:27 : Calculating goodness of fit...

Finished k-means clustering on "madlib"."kmeans_input" table. 
Results: 
    * 10 centroids (goodness of fit = 2.9787570001).
Output:
    * table : "madlib"."kmeans_out_centroids_testrun"
    * table : "madlib"."kmeans_out_points_testrun"
Time elapsed: 0 minutes 4.335644 seconds.@endverbatim

\par To Do:

- add support for dense arrays (now it's only SVEC)
- allow the method to start processing a source table w/o a "PointID" column 
    (now it needs both ID and POSITION columns)
- fix the "goodness of fit" test to be scale, rotation and transition invariant
- rewrite the following plpgsql stored procedure into C:
    - func: _kmeans_bestCentroid
    - agg: _kmeans_meanPosition

"""
