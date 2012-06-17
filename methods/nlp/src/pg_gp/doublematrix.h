/*
 * Copyright (C) 2004 - 2005 by
 *     Hieu Xuan Phan & Minh Le Nguyen {hieuxuan, nguyenml}@jaist.ac.jp
 *     Graduate School of Information Science,
 *     Japan Advanced Institute of Science and Technology (JAIST)
 *
 * doublematrix.h - this file is part of FlexCRFs.
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

#ifndef _DOUBLEMATRIX_H
#define _DOUBLEMATRIX_H

#include <iostream>

using namespace std;

class doublematrix {
public:
    double ** mtrx;	// matrix content
    int rows, cols;	// number of rows and columns
    
    // default constructor
    doublematrix() {
	rows = cols = 0;
	mtrx = NULL;
    }

    // constructor with rows and cols    
    doublematrix(int rows, int cols);
    
    // constructor with rows, cols and matrix content
    doublematrix(int rows, int cols, double ** mtrx);

    // copy constructor
    doublematrix(doublematrix & dm);
    
    // destructor
    ~doublematrix() {
	if (mtrx) {
	    for (int i = 0; i < rows; i++) {
		delete mtrx[i];
	    }
	    delete mtrx;
	}
    }
    
    // overloading assignment operator (with one value)
    void operator=(double val);
    
    // overloading assignment operator
    void operator=(doublematrix & dm);
    
    // assign the same value for all elements
    void assign(double val);
    
    // assign values for elements from values from another matrix
    void assign(doublematrix & dm);
    
    // reference to an element
    double & get(int i, int j);
};

#endif

