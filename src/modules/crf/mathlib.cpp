/*
 * Copyright (C) 2004 - 2005 by
 *     Hieu Xuan Phan & Minh Le Nguyen {hieuxuan, nguyenml}@jaist.ac.jp
 *     Graduate School of Information Science,
 *     Japan Advanced Institute of Science and Technology (JAIST)
 *
 * mathlib.cpp - this file is part of FlexCRFs.
 *
 * Begin:	Dec. 15, 2004
 * Last change:	Sep. 09, 2005
 *
 * FlexCRFs is a free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * FlexCRFs is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FlexCRFs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include "mathlib.h"

void mathlib::mult(int size, doublevector * x, doublematrix * A, doublevector * y, 
		    int is_transposed) {
    // is_transposed = 0 (false): 	x = A * y
    // is_transposed = 1 (true):	x^t = y^t * A^t
    // in which x^t, y^t, and A^t are the transposed vectors and matrix
    // of x, y, and A, respectively 
    
    int i, j;
    
    if (!is_transposed) {
	// for beta
	// x = A * y
	for (i = 0; i < size; i++) {
	    x->vect[i] = 0;
	    for (j = 0; j < size; j++) {
		x->vect[i] += A->mtrx[i][j] * y->vect[j];
	    }
	}
	
    } else {
	// for alpha
	// x^t = y^t * A^t
	for (i = 0; i < size; i++) {
	    x->vect[i] = 0;
	    for (j = 0; j < size; j++) {
		x->vect[i] += y->vect[j] * A->mtrx[j][i];
	    }
	}
    }
}

