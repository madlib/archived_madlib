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

class mathlib {
public:
    static void mult(int size, doublevector * x, doublematrix * A,
                     doublevector * y, int is_transposed = 0);
};

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

class doublematrix {
public:
    double ** mtrx;     // matrix content
    int rows, cols;     // number of rows and columns

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

class doublevector {
public:
    double * vect;	// vector content
    int len;		// vector length	
    
    // default constructor
    doublevector() {
	len = 0; 
	vect = NULL;
    }
    
    // constructor with length
    doublevector(int len);
    
    // constructor with length and content
    doublevector(int len, double * vect);
    
    // copy constructor
    doublevector(doublevector & dv);
    
    // destructor
    ~doublevector() {
	if (vect) {
	    delete vect;
	}
    }
    
    // the size of vector
    int size();
    
    // overloading assignment operator
    void operator=(double val);
    
    // overloading assignment operator
    void operator=(doublevector & dv);
    
    // assign the same value to all vector elements
    void assign(double val);
    
    // assign values for all elements from another doublevector
    void assign(doublevector & dv);
    
    // reference to an element of index "idx"
    double & operator[](int idx);
    
    // sum of all vector elements
    double sum();
    
    // component multiplication
    void comp_mult(double val);
    
    // component multiplication
    void comp_mult(doublevector * dv);
};

using namespace std;

// constructor with length
doublevector::doublevector(int len) {
    this->len = len;
    vect = new double[len];
}
    
// constructor with length and content
doublevector::doublevector(int len, double * vect) {
    this->len = len;
    vect = new double[len];
    for (int i = 0; i < len; i++) {
        this->vect[i] = vect[i];
    }	    
}
    
// copy constructor
doublevector::doublevector(doublevector & dv) {
    len = dv.len;
    vect = new double[len];
    for (int i = 0; i < len; i++) {
        vect[i] = dv.vect[i];
    }
}
    
// the size of vector
int doublevector::size() {
    return len;
}
    
// overloading assignment operator
void doublevector::operator=(double val) {
    for (int i = 0; i < len; i++) {
        vect[i] = val;
    }
}
    
// overloading assignment operator
void doublevector::operator=(doublevector & dv) {
    if (len != dv.len) {
        if (vect) {
	    delete vect;
	}
	len = dv.len;
	vect = new double[len];	    
    }
	
    for (int i = 0; i < len; i++) {
        vect[i] = dv.vect[i];
    }
}    
    
// assign the same value to all vector elements
void doublevector::assign(double val) {
    *this = val;
}
    
// assign values for all elements from another doublevector
void doublevector::assign(doublevector & dv) {
    *this = dv;
}
    
// reference to an element of index "idx"
double & doublevector::operator[](int idx) {
    return vect[idx];
}
    
// sum of all vector elements
double doublevector::sum() {
    double res = 0.0;
    for (int i = 0; i < len; i++) {
        res += vect[i];
    }
    return res;
}

// component multiplication
void doublevector::comp_mult(double val) {
    for (int i = 0; i < len; i++) {
	vect[i] *= val;
    }
}

// component multiplication
void doublevector::comp_mult(doublevector * db) {
    for (int i = 0; i < len; i++) {
	vect[i] *= db->vect[i];
    }
}



