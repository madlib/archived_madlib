/*
 * Copyright (C) 2004 - 2005 by
 *     Hieu Xuan Phan & Minh Le Nguyen {hieuxuan, nguyenml}@jaist.ac.jp
 *     Graduate School of Information Science,
 *     Japan Advanced Institute of Science and Technology (JAIST)
 *
 * doublematrix.cpp - this file is part of FlexCRFs.
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

#include "doublematrix.h"

using namespace std;

// constructor with rows and cols    
doublematrix::doublematrix(int rows, int cols) {
    this->rows = rows;
    this->cols = cols;
	
    mtrx = new double*[rows];
    for (int i = 0; i < rows; i++) {
        mtrx[i] = new double[cols];
    }
}
    
// constructor with rows, cols and matrix content
doublematrix::doublematrix(int rows, int cols, double ** mtrx) {
    this->rows = rows;
    this->cols = cols;
    this->mtrx = new double*[rows];
    for (int i = 0; i < rows; i++) {
        this->mtrx[i] = new double[cols];
    
        for (int j = 0; j < cols; j++) {
    	    this->mtrx[i][j] = mtrx[i][j];
	}
    }	
}
    
// copy constructor
doublematrix::doublematrix(doublematrix & dm) {
    rows = dm.rows;
    cols = dm.cols;
    mtrx = new double*[rows];
    for (int i = 0; i < rows; i++) {
        mtrx[i] = new double[cols];
	    
        for (int j = 0; j < cols; j++) {
	    mtrx[i][j] = dm.mtrx[i][j];
	}
    }
}
    
// overloading assignment operator (with one value)
void doublematrix::operator=(double val) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
	    mtrx[i][j] = val;
	}
    }
}
    
// overloading assignment operator
void doublematrix::operator=(doublematrix & dm) {
    int i, j;
	
    if (rows != dm.rows || cols != dm.cols) {
        for (i = 0; i < rows; i++) {
	    delete mtrx[i];		
	}    
	delete mtrx;
	    
	rows = dm.rows;
	cols = dm.cols;
	mtrx = new double*[rows];
	for (i = 0; i < rows; i++) {
	    mtrx[i] = new double[cols];
	}
    }
	
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
	    mtrx[i][j] = dm.mtrx[i][j];
	}
    }
}
    
// assign the same value for all elements
void doublematrix::assign(double val) {
    *this = val;
}
    
// assign values for elements from values from another matrix
void doublematrix::assign(doublematrix & dm) {
    *this = dm;
}
    
// reference to an element
double & doublematrix::get(int i, int j) {
    return mtrx[i][j];
}

