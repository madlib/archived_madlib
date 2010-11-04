/*-------------------------------------------------------------------------
 *
 * cephes.h
 *
 * MADlib interface to CEPHES library, v2.8, by Stephen L. Moshier.
 * Source: http://www.moshier.net/
 * See http://www.netlib.org/cephes/readme for usage terms and conditions.
 *
 *------------------------------------------------------------------------- 
 */

#ifndef MADLIB_CEPHES_H
#define MADLIB_CEPHES_H

double incbet ( double, double, double );
int mtherr ( char *name, int code );
double stdtr ( int k, double t );
double stdtri ( int k, double p );

#endif
