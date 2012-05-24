/*
 * Copyright (C) 2004 - 2005 by
 *     Hieu Xuan Phan & Minh Le Nguyen {hieuxuan, nguyenml}@jaist.ac.jp
 *     Graduate School of Information Science,
 *     Japan Advanced Institute of Science and Technology (JAIST)
 *
 * trainer.h - this file is part of FlexCRFs.
 *
 * Begin:	Dec. 15, 2004
 * Last change:	Nov. 01, 2005
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

#ifndef	_TRAINER_H
#define	_TRAINER_H

#include <fstream>
#include "data.h"
#include "feature.h"
#include "featuregen.h"
#include "dictionary.h"
#include "option.h"
#include "doublevector.h"
#include "doublematrix.h"

using namespace std;

class model;

class trainer {
public:
    model * pmodel;	// pointer to conditional random field object
    option * popt;	// .......... option object
    data * pdata;	// .......... data object
    dictionary * pdict;	// .......... dictionary object
    featuregen * pfgen;	// .......... featuregen object
    
    int num_labels;
    int num_features;
    double * lambda;
    double * temp_lambda;
    int is_logging;
    
    double * gradlogli;	// log-likelihood vector gradient
    double * diag;	// for optimization (used by L-BFGS)
    
    doublematrix * Mi;	// for edge features (a small modification from published papers)
    doublevector * Vi;	// for state features
    doublevector * alpha, * next_alpha;	// forward variable
    vector<doublevector *> betas;	// backward variables
    doublevector * temp;	// temporary vector used during computing
    
    double * ExpF;	// feature expectation (according to the model)
    double * ws;	// memory workspace used by L-BFGS
    
    // for scaling (to avoid numerical problems during training)
    vector<double> scale, rlogscale;
    
    // this control the status information reported during training
    int * iprint;
    
    trainer();
    
    ~trainer();
    
    // initialization
    void init();
    
    // compute norm of a vector
    static double norm(int len, double * vect);
    
    // training
    void train(FILE * fout);
    
    // compute log-likelihood vector gradient 
    double compute_logli_gradient_1order(double * lambda, double * gradlogli,
		int num_iters, FILE * fout);
		
    // compute log Mi
    void compute_log_Mi_1order(sequence & seq, int pos, doublematrix * Mi,
		doublevector * Vi, int is_exp);

    // compute log-likelihood vector gradient 
    double compute_logli_gradient_2order(double * lambda, double * gradlogli,
		int num_iters, FILE * fout);

    // compute log Mi
    void compute_log_Mi_2order(sequence & seq, int pos, doublematrix * Mi,
		doublevector * Vi, int is_exp);
};

#endif

