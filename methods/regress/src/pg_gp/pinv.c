#include "pinv.h"

#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

Datum 
pseudoinverse(PG_FUNCTION_ARGS)
{
	long int    rows, columns;
	float8     *A, *Aplus;
	ArrayType  *A_PG, *Aplus_PG;
	int lbs[2], dims[2];

	/* 
	 * Perform all the error checking needed to ensure that no one is
	 * trying to call this in some sort of crazy way. 
	 */
	if (PG_NARGS() != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pseudoinverse called with %d arguments", 
						PG_NARGS())));
	}
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();	

	A_PG = PG_GETARG_ARRAYTYPE_P(0);

	if (ARR_ELEMTYPE(A_PG) != FLOAT8OID) 
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pseudoinverse only defined over float8[]")));
	if (ARR_NDIM(A_PG) != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pseudoinverse only defined over 2 dimensional arrays"))
			);
	if (ARR_NULLBITMAP(A_PG)) 
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("null array element not allowed in this context")));

	/* Extract rows, columns, and data */
	rows = ARR_DIMS(A_PG)[0];
	columns = ARR_DIMS(A_PG)[1];
	A = (float8 *) ARR_DATA_PTR(A_PG);


	/*  Allocate a PG array for the result, "Aplus" is A+, the pseudo inverse of A */
	lbs[0] = 1; lbs[1] = 1;
	dims[0] = columns; dims[1] = rows;

#ifdef GP_VERSION
	/* Greenplum allows the first argument of construct_md_array to be NULL,
	   in which case the default value is used for the new array. */
	Aplus_PG = construct_md_array(NULL, NULL, 2, dims, lbs, FLOAT8OID, 
								  sizeof(float8), true, 'd');
#else
	/* In PostgreSQL, passing NULL for the first arguement will lead to a segfault. */
	Aplus = (float8 *) palloc(rows * columns * sizeof(float8));
	Aplus_PG = construct_md_array((Datum *) Aplus, NULL, 2, dims, lbs, FLOAT8OID, 
								  sizeof(float8), true, 'd');
	pfree(Aplus); /* clean up because construct_md_array copies into a new array. */
#endif
	
	Aplus = (float8 *) ARR_DATA_PTR(Aplus_PG);

	pinv(rows,columns,A,Aplus);

	PG_RETURN_ARRAYTYPE_P(Aplus_PG);
}

/*

    float8[] *pseudoinverse(float8[])

    Compute the pseudo inverse of matrix A

    Author:  Luke Lonergan
    Date:    5/31/08
    License: Use pfreely

    We use the approach from here:
       http://en.wikipedia.org/wiki/Moore-Penrose_pseudoinverse#Finding_the_\
pseudoinverse_of_a_matrix

    Synopsis:
       A computationally simpler and more accurate way to get the pseudoinverse 
	   is by using the singular value decomposition.[1][5][6] If A = U Σ V* is 
	   the singular value decomposition of A, then A+ = V Σ+ U* . For a diagonal
       matrix such as Σ, we get the pseudoinverse by taking the reciprocal of 
	   each non-zero element on the diagonal, and leaving the zeros in place. 
	   In numerical computation, only elements larger than some small tolerance 
	   are taken to be nonzero, and the others are replaced by zeros. For 
	   example, in the Matlab function pinv, the tolerance is taken to be
	   t = ε•max(rows,columns)•max(Σ), where ε is the machine epsilon.

    Input:  the matrix A with "rows" rows and "columns" columns, in column 
	        values consecutive order
    Output: the matrix A+ with "columns" rows and "rows" columns, the 
	        Moore-Penrose pseudo inverse of A

    The approach is summarized:
    - Compute the SVD (diagonalization) of A, yielding the U, S and V 
      factors of A
    - Compute the pseudo inverse A+ = U x S+ x Vt

    S+ is the pseudo inverse of the diagonal matrix S, which is gained by 
	inverting the non zero diagonals 

    Vt is the transpose of V

    Note that there is some fancy index rework in this implementation to deal 
	with the row values consecutive order used by the FORTRAN dgesdd_ routine.
*/
void pinv(integer rows, integer columns, float8 *A, float8 *Aplus)
{
	long int    minmn;
	integer    i, j, k, ii;
	integer    lwork, *iwork;
	float8     *work, *Atemp;
	float8      epsilon, tolerance, maxeigen;
	float8     *S, *U, *Vt;
	float8     *Splus, *Splus_times_Ut;
	char        achar='A';   /* ? */

	/* 
	 * Calculate the tolerance for "zero" values in the SVD 
	 *    t = ε•max(rows,columns)•max(Σ) 
     *  (Need to multiply tolerance by max of the eigenvalues when they're 
	 *   available)
	 */
	epsilon = pow(2,1-56); 
	tolerance = epsilon * max(rows,columns); 
	maxeigen=-1.;

	/*
	 * The factors of A: S, U and Vt
	 * U, Sdiag and Vt are the factors of the pseudo inverse of A, the 
	 * components of the singular value decomposition of A
	 */
	S = (float8 *) palloc(sizeof(float8)*min(rows,columns));
	U = (float8 *) palloc(sizeof(float8)*rows*rows);
	Vt = (float8 *) palloc(sizeof(float8)*columns*columns);

	/* Working matrices for the pseudo inverse calculation: */
	/*  1) The pseudo inverse of S: S+ */
	Splus = (float8 *) palloc(sizeof(float8)*columns*rows);
	/*  2) An intermediate result: S+ Ut */
	Splus_times_Ut = (float8 *) palloc(sizeof(float8)*columns*rows);

	/*
	 * Here we transpose A for entry into the FORTRAN dgesdd_ routine in row 
	 * order. Note that dgesdd_ is destructive to the entry array, so we'd 
	 * need to make this copy anyway.
	 */
	Atemp = (float8 *) palloc(sizeof(float8)*columns*rows);
	for ( j = 0; j < rows; j++ ) {
			for ( i = 0; i < columns; i++ ) {
					Atemp[j+i*rows] = A[i+j*columns];
			} 
	}

	/* 
	 * First call of dgesdd is with lwork=-1 to calculate an optimal value of 
	 * lwork 
	 */
	iwork = (integer *) palloc(sizeof(long int)*8*min(rows,columns));
	lwork=-1;
	
	/* Need a single location in work to store the recommended value of lwork */
	work = (float8 *)palloc(sizeof(float8)*1);
#ifdef WIN32
	elog(ERROR,"pseudoinverse: lanpack routine dgesdd not available on WIN32");
#else
	dgesdd_( &achar, &rows, &columns, Atemp, &rows, S, U, &rows, Vt, &columns, 
			 work, &lwork, iwork, &i );

	if (i != 0) {
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pseudoinverse: lanpack routine dgesdd returned error"))
			);
	} else {
		lwork = (int) work[0];
		pfree(work);
	}


    /*
	 * Allocate the space needed for the work array using the value of lwork 
	 * obtained in the first call of dgesdd_ 
	 */
	work = (float8 *) palloc(sizeof(float8)*lwork);
	dgesdd_( &achar, &rows, &columns, Atemp, &rows, S, U, &rows, Vt, &columns, 
			 work, &lwork, iwork, &i );

#endif

	pfree(work);
	pfree(iwork);
	pfree(Atemp);

	/* Use the max of the eigenvalues to normalize the zero tolerance */
	minmn = min(rows,columns); // The dimensions of S are min(rows,columns)
	for ( i = 0; i < minmn; i++ ) {
		maxeigen = max(maxeigen,S[i]);
	}
	tolerance *= maxeigen;

	/*
	 * Calculate the pseudo inverse of the eigenvalue matrix, Splus
	 * Use a tolerance to evaluate elements that are close to zero
	 */
	for ( j = 0; j < rows; j++ ) {
		for ( i = 0; i < columns; i++ ) {
			if (minmn == columns) {
				ii = i;
			} else {
				ii = j;
			}
			if ( i == j && S[ii] > tolerance ) {
				Splus[i+j*columns] = 1.0 / S[ii];
			} else {
				Splus[i+j*columns] = 0.0;
			} 
		} 
	}
	
	for ( i = 0; i < columns; i++ ) {
		for ( j = 0; j < rows; j++ ) {
			Splus_times_Ut[i+j*columns] = 0.0;
			for ( k = 0; k < rows; k++ ) {
				Splus_times_Ut[i+j*columns] = 
					Splus_times_Ut[i+j*columns] + 
					Splus[i+k*columns] * U[j+k*rows];
			} 
		} 
	}

	for ( i = 0; i < columns; i++ ) {
		for ( j = 0; j < rows; j++ ) {
			Aplus[j+i*rows] = 0.0;
			for ( k = 0; k < columns; k++ ) {
				Aplus[j+i*rows] =
					Aplus[j+i*rows] + 
					Vt[k+i*columns] * Splus_times_Ut[k+j*columns];
			} 
		} 
	}

	pfree(Splus);
	pfree(Splus_times_Ut);
	pfree(U);
	pfree(Vt);
	pfree(S);

	return;
}
