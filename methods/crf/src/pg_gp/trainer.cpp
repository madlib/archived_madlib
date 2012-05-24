/*
 * Copyright (C) 2004 - 2005 by
 *     Hieu Xuan Phan & Minh Le Nguyen {hieuxuan, nguyenml}@jaist.ac.jp
 *     Graduate School of Information Science,
 *     Japan Advanced Institute of Science and Technology (JAIST)
 *
 * trainer.cpp - this file is part of FlexCRFs.
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

#include <stdio.h>
#include <math.h>
#include <time.h>
#include "../../../include/trainer.h"
#include "../../../include/model.h"
#include "../../../include/evaluation.h"
#include "../../../include/mathlib.h"

using namespace std;

/*--------------------------------------------------------------------------------*/
extern "C" {
    // interface to LBFGS optimization written in FORTRAN
    extern void lbfgs(int * n, int * m, double * x, double * f, double * g,
		       int * diagco, double * diag, int * iprint, double * eps,
		       double * xtol, double * w, int * iflag);		       
}
/*--------------------------------------------------------------------------------*/

// destructor
trainer::trainer() {
}

// destructor
trainer::~trainer() {
    if (gradlogli) { delete gradlogli; }    
    if (diag) {	delete diag; }    
    
    if (Mi) { delete Mi; }
    if (Vi) { delete Vi; }
    if (alpha) { delete alpha; }
    if (next_alpha) { delete next_alpha; }
    if (temp) { delete temp; }
    for (int i = 0; i < betas.size(); i++) { delete betas[i]; }
    
    if (ExpF) { delete ExpF; }
    if (ws) { delete ws; }
    
    if (iprint) { delete iprint; }
    
    if (temp_lambda) {
	delete temp_lambda;
    }
}

// initialize
void trainer::init() {
    popt = pmodel->popt;
    pdata = pmodel->pdata;
    pdict = pmodel->pdict;
    pfgen = pmodel->pfgen;

    if (popt->order == FIRST_ORDER) {    
	num_labels = popt->num_labels;
    } else if (popt->order == SECOND_ORDER) {
	num_labels = popt->num_2orderlabels;
    }
    
    num_features = popt->num_features;
    lambda = pmodel->lambda;	    
    temp_lambda = new double[num_features];
    is_logging = popt->is_logging;
            
    // allocate memory for vector gradient of log-likelihood function
    gradlogli = new double[num_features];
    // diag is only for LBFGS optimization
    diag = new double[num_features];
    
    Mi = new doublematrix(num_labels, num_labels);
    Vi = new doublevector(num_labels);
    
    alpha = new doublevector(num_labels);
    next_alpha = new doublevector(num_labels);
    temp = new doublevector(num_labels);

    // allocate memory for vector of feature expectations    
    ExpF = new double[num_features];    

    // allocate memory for ws (workspace)
    // this memory is only for LBFGS optimization
    int ws_size = num_features * (2 * popt->m_for_hessian + 1) + 
		  2 * popt->m_for_hessian;
    ws = new double[ws_size];
        
    iprint = new int[2];
}

// compute norm of a vector
double trainer::norm(int len, double * vect) {
    double res = 0.0;
    for (int i = 0; i < len; i++) {
	res += vect[i] * vect[i];
    }
    return sqrt(res);
}

// the training method
void trainer::train(FILE * fout) {
    // for timing
    time_t s_train_time, e_train_time;
    time_t s_iter_time, e_iter_time;
    int hours, minutes, seconds;

    // initialization
    init();

    double f = 0.0;	// log-likelihood function value
    double old_f;	// only for tracking the termination condition
    double xtol = 1.0e-16; // machine precision
    int num_iters = 0;	// the iteration counter

    // for lbfgs optimization
    iprint[0] = popt->debug_level - 2;
    iprint[1] = popt->debug_level - 1;

    int iflag = 0;
    
    // indicate whether or not user provide the diagonal matrix Hk0
    // at each iteration (here we chose "not provide", i.e., diagco = false)
    int diagco = 0;	 
    
    // counter variable
    int i;
    
    // get initial values for lambda components
    for (i = 0; i < num_features; i++) {
	lambda[i] = popt->init_lambda_val;
	temp_lambda[i] = popt->init_lambda_val;
    }

    // get the start time
    s_train_time = time(NULL);
    
    // logging
    if (is_logging) {
	popt->write_options(fout);
	fprintf(fout, "Start to train ...\n");
    }

    double max_f1 = 0.0;
    int max_iter = -1;
    
    // the main loop for training CRF
    do {	
	// get the start time of the current iteration
	s_iter_time = time(NULL);
    
	// call this function to compute two things:
	// 1. the value of log likelihood of the current lambda
	// 2. the gradient component of the log likelihood function
	if (popt->order == FIRST_ORDER) {	
	    f = compute_logli_gradient_1order(lambda, gradlogli, num_iters + 1, fout);
	} else if (popt->order == SECOND_ORDER) {
	    f = compute_logli_gradient_2order(lambda, gradlogli, num_iters + 1, fout);
	}
		
	// negate f and gradient vector because the LBFGS optimization below minimizes 
	// the ojective function while we would like to maximize it
	f *= -1;
	for (i = 0; i < num_features; i++) {
	    gradlogli[i] *= -1;
	}

	/*
	extern void lbfgs(int * n, int * m, double * x, double * f, double * g,
		       int & diagco, double * diag, int * iprint, double * eps,
		       double * xtol, double * w, int * iflag);		       
	*/	
	// calling LBFGS optimization routine
	lbfgs(&num_features, &(popt->m_for_hessian), lambda, &f, gradlogli, &diagco, 
		diag, iprint, &(popt->eps_for_convergence), &xtol, ws, &iflag);
	
	// checking after calling LBFGS	
	if (iflag < 0) {
	    // LBFGS error
	    printf("LBFGS routine encounters an error\n");
	    if (is_logging) {
		fprintf(fout, "LBFGS routine encounters an error\n");
	    }
	    break;
	}
	
	// increase the iteration counter
	num_iters++;	

	// get the end time of the current iteration
	e_iter_time = time(NULL);
	e_iter_time -= s_iter_time;
	// display the elapsed time
	printf("\tIteration elapsed: %d seconds\n", e_iter_time);
	if (is_logging) {
	    fprintf(fout, "\tIteration elapsed: %d seconds\n", e_iter_time);
	}
	
	// evaluate during training
	if (popt->evaluate_during_training) {
	    pmodel->apply_tstdata();
	    printf("\n");
	    double total_f1 = pmodel->peval->evaluate(fout);
	    if (total_f1 > max_f1) {
		max_f1 = total_f1;
		max_iter = num_iters;
		
		for (i = 0; i < num_features; i++) {
		    temp_lambda[i] = lambda[i];
		}
	    }
	    
	    if (popt->chunk_evaluate_during_training) {
		printf("\tCurrent max chunk-based F1: %6.2f (iteration %d)\n",
			max_f1, max_iter);
	    } else {
		printf("\tCurrent max tag-based F1: %6.2f (iteration %d)\n",
			max_f1, max_iter);
	    }
	    if (is_logging) {
		fprintf(fout, "\n");
	    
		if (popt->chunk_evaluate_during_training) {
		    fprintf(fout, "\tCurrent max chunk-based F1: %6.2f (iteration %d)\n",
			max_f1, max_iter);
		} else {
		    fprintf(fout, "\tCurrent max tag-based F1: %6.2f (iteration %d)\n",
			max_f1, max_iter);
		}
	    }
	    
	    // get the end time of the current iteration
	    e_iter_time = time(NULL);
	    e_iter_time -= s_iter_time;
	    // display the elapsed time
	    printf("\tTraining iteration elapsed (including testing & evaluation time): %d seconds\n",
		    e_iter_time);
	    if (is_logging) {
		fprintf(fout, "\tTraining iteration elapsed (including testing & evaluation time): %d seconds\n",
		    e_iter_time);
	    }    
	}

    } while (iflag != 0 && num_iters < popt->num_iterations);
    
    // get the end time
    e_train_time = time(NULL);
    e_train_time -= s_train_time;
    // display the training time
    printf("\nThe training process elapsed: %d seconds\n\n", e_train_time);
    if (is_logging) {
	fprintf(fout, "\nThe training process elapsed: %d seconds\n\n", e_train_time);
    }
    
    if (popt->evaluate_during_training) {
	for (i = 0; i < num_features; i++) {
	    lambda[i] = temp_lambda[i];
	}
    }
}

// compute log-likelihood gradient vector (first-order Markov)
double trainer::compute_logli_gradient_1order(double * lambda, double * gradlogli, 
			int num_iters, FILE * fout) {
    double logli = 0.0;
    
    // counter variable
    int i, j, k;
    
    for (i = 0; i < num_features; i++) {
	gradlogli[i] = -1 * lambda[i] / popt->sigma_square;
	logli -= (lambda[i] * lambda[i]) / (2 * popt->sigma_square);
    }
    
    dataset::iterator datait;
    sequence::iterator seqit;
    
    int seq_count = 0;
    // go though all training data sequences
    for (datait = pdata->ptrndata->begin(); datait != pdata->ptrndata->end(); datait++) {
	seq_count++;
	int seq_len = datait->size();
	
	*alpha = 1;
	
	for (i = 0; i < num_features; i++) {
	    ExpF[i] = 0;
	}
	
	int betassize = betas.size();
	if (betassize < seq_len) {
	    // allocate more beta vector		
	    for (i = 0; i < seq_len - betassize; i++) {
		betas.push_back(new doublevector(num_labels));
	    }	    
	}
	
	int scalesize = scale.size();
	if (scalesize < seq_len) {
	    // allocate more scale elements
	    for (i = 0; i < seq_len - scalesize; i++) {
		scale.push_back(1.0);
	    }
	}
	
	// compute beta values in a backward fashion
	// also scale beta-values to 1 to avoid numerical problems
	scale[seq_len - 1] = (popt->is_scaling) ? num_labels : 1;
	betas[seq_len - 1]->assign(1.0 / scale[seq_len - 1]);
	
	// start to compute beta values in backward fashion
	for (int i = seq_len - 1; i > 0; i--) {
	    // compute the Mi matrix and Vi vector
	    compute_log_Mi_1order(*datait, i, Mi, Vi, 1);
	    *temp = *(betas[i]);
	    temp->comp_mult(Vi);
	    mathlib::mult(num_labels, betas[i - 1], Mi, temp, 0);
	    
	    // scale for the next (backward) beta values
	    scale[i - 1] = (popt->is_scaling) ? betas[i - 1]->sum() : 1;
	    betas[i - 1]->comp_mult(1.0 / scale[i - 1]);
	} // end of beta values computation
	
	// start to compute the log-likelihood of the current sequence
	double seq_logli = 0;
	for (j = 0; j < seq_len; j++) {
	    compute_log_Mi_1order(*datait, j, Mi, Vi, 1);
	    
	    if (j > 0) {
		*temp = *alpha;
		mathlib::mult(num_labels, next_alpha, Mi, temp, 1);
		next_alpha->comp_mult(Vi);
	    } else {
		*next_alpha = *Vi;
	    }
	    
	    // start to scan feature at "i" position of the current sequence
	    pfgen->start_scan_features_at(*datait, j);
	    while (pfgen->has_next_feature()) {
		feature f;
		pfgen->next_feature(f);
		
		if ((f.ftype == EDGE_FEATURE1 && f.y == (*datait)[j].label && 
			(j > 0 && f.yp == (*datait)[j-1].label)) || 
			(f.ftype == STAT_FEATURE1 && f.y == (*datait)[j].label)) {
		    gradlogli[f.idx] += f.val;
		    seq_logli += lambda[f.idx] * f.val;		    
		}
		
		if (f.ftype == STAT_FEATURE1) {
		    // state feature
		    ExpF[f.idx] += (*next_alpha)[f.y] * f.val * (*(betas[j]))[f.y];
		} else if (f.ftype == EDGE_FEATURE1) {
		    // edge feature
		    ExpF[f.idx] += (*alpha)[f.yp] * (*Vi)[f.y] * Mi->mtrx[f.yp][f.y] 
				    * f.val * (*(betas[j]))[f.y];
		}		
	    }	    
	    
	    *alpha = *next_alpha;
	    alpha->comp_mult(1.0 / scale[j]);	    
	} 

	// Zx = sum(alpha_i_n) where i = 1..num_labels, n = seq_len
	double Zx = alpha->sum();
	
	// Log-likelihood of the current sequence
	// seq_logli = lambda * F(y_k, x_k) - log(Zx_k)
	// where x_k is the current sequence
	seq_logli -= log(Zx);
	
	// re-correct the value of seq_logli because Zx was computed from
	// scaled alpha values
	for (k = 0; k < seq_len; k++) {
	    seq_logli -= log(scale[k]);
	}

	// Log-likelihood = sum_k[lambda * F(y_k, x_k) - log(Zx_k)]
	logli += seq_logli;
	
	// update the gradient vector
	for (k = 0; k < num_features; k++) {
	    gradlogli[k] -= ExpF[k] / Zx;
	}

    } // end of the main loop

    // output some status information
    if (popt->debug_level > 0) {
	// cout << endl;
	printf("\n");
	printf("Iteration: %d\n", num_iters);
	printf("\tLog-likelihood                       = %17.6f\n", logli);
	double gradlogli_norm = trainer::norm(num_features, gradlogli);
	printf("\tNorm(log-likelihood gradient vector) = %17.6f\n", gradlogli_norm);		
	double lambda_norm = trainer::norm(num_features, lambda);    
	printf("\tNorm(lambda vector)                  = %17.6f\n", lambda_norm);
		
	if (is_logging) {
	    fprintf(fout, "\n");
	    fprintf(fout, "Iteration: %d\n", num_iters);
	    fprintf(fout, "\tLog-likelihood                       = %17.6f\n", logli);
	    fprintf(fout, "\tNorm(log-likelihood gradient vector) = %17.6f\n", gradlogli_norm);		
	    fprintf(fout, "\tNorm(lambda vector)                  = %17.6f\n", lambda_norm);
	}
    }    
    
    return logli;
}

// compute log Mi (first-order Markov)
void trainer::compute_log_Mi_1order(sequence & seq, int pos, doublematrix * Mi, 
		  doublevector * Vi, int is_exp) {
    *Mi = 0.0;
    *Vi = 0.0;

    // start scan features for sequence "seq" at position "i"
    pfgen->start_scan_features_at(seq, pos);
    // examine all features at position "pos"
    while (pfgen->has_next_feature()) {
	feature f;
	pfgen->next_feature(f);
	
	if (f.ftype == STAT_FEATURE1) {
	    // state feature
	    (*Vi)[f.y] += lambda[f.idx] * f.val;
	} else if (f.ftype == EDGE_FEATURE1) /* if (pos > 0)*/ {
	    // edge feature (i.e., f.ftype == EDGE_FEATURE)
	    Mi->get(f.yp, f.y) += lambda[f.idx] * f.val;
	}
    }
    
    // take exponential operator
    if (is_exp) {
	for (int i = 0; i < Mi->rows; i++) {
	    // update for Vi
	    (*Vi)[i] = exp((*Vi)[i]);
	    // update for Mi
	    for (int j = 0; j < Mi->cols; j++) {
		Mi->get(i, j) = exp(Mi->get(i, j));
	    }
	}
    }
}

// compute log-likelihood gradient vector (second-order Markov)
double trainer::compute_logli_gradient_2order(double * lambda, double * gradlogli, 
			int num_iters, FILE * fout) {
    double logli = 0.0;
    
    int lfo = popt->num_labels - 1;
    if (popt->lfo >= 0) {
	lfo = popt->lfo;
    }
    
    // counter variable
    int i, j, k;
    
    for (i = 0; i < num_features; i++) {
	gradlogli[i] = -1 * lambda[i] / popt->sigma_square;
	logli -= (lambda[i] * lambda[i]) / (2 * popt->sigma_square);
    }
    
    dataset::iterator datait;
    sequence::iterator seqit;
    
    int seq_count = 0;
    // go though all training data sequences
    for (datait = pdata->ptrndata->begin(); datait != pdata->ptrndata->end(); datait++) {
	seq_count++;
	int seq_len = datait->size();
	
	*alpha = 1;
	
	for (i = 0; i < num_features; i++) {
	    ExpF[i] = 0;
	}
	
	int betassize = betas.size();
	if (betassize < seq_len) {
	    // allocate more beta vector		
	    for (i = 0; i < seq_len - betassize; i++) {
		betas.push_back(new doublevector(num_labels));
	    }	    
	}
	
	int scalesize = scale.size();
	if (scalesize < seq_len) {
	    // allocate more scale elements
	    for (i = 0; i < seq_len - scalesize; i++) {
		scale.push_back(1.0);
	    }
	}
	
	// compute beta values in a backward fashion
	// also scale beta-values to 1 to avoid numerical problems
	scale[seq_len - 1] = (popt->is_scaling) ? num_labels : 1;
	betas[seq_len - 1]->assign(1.0 / scale[seq_len - 1]);
	
	// start to compute beta values in backward fashion
	for (int i = seq_len - 1; i > 0; i--) {
	    // compute the Mi matrix and Vi vector
	    compute_log_Mi_2order(*datait, i, Mi, Vi, 1);
	    *temp = *(betas[i]);
	    temp->comp_mult(Vi);
	    mathlib::mult(num_labels, betas[i - 1], Mi, temp, 0);
	    
	    // scale for the next (backward) beta values
	    scale[i - 1] = (popt->is_scaling) ? betas[i - 1]->sum() : 1;
	    betas[i - 1]->comp_mult(1.0 / scale[i - 1]);
	} // end of beta values computation
	
	// start to compute the log-likelihood of the current sequence
	double seq_logli = 0;
	for (j = 0; j < seq_len; j++) {
	    compute_log_Mi_2order(*datait, j, Mi, Vi, 1);
	    
	    if (j > 0) {
		*temp = *alpha;
		mathlib::mult(num_labels, next_alpha, Mi, temp, 1);
		next_alpha->comp_mult(Vi);
	    } else {
		*next_alpha = *Vi;
	    }
	    
	    // start to scan feature at "i" position of the current sequence
	    pfgen->start_scan_features_at(*datait, j);
	    while (pfgen->has_next_feature()) {
		feature f;
		pfgen->next_feature(f);
		
		if (f.ftype == EDGE_FEATURE1 && f.y == (*datait)[j].label) {
		    // edge feature (type 1)    
		    if ((j == 0 && f.yp == lfo) || 
			    (j > 0  && f.yp == (*datait)[j-1].label)) {
			gradlogli[f.idx] += f.val;
			seq_logli += lambda[f.idx] * f.val;		    
		    }
		
		} else if (f.ftype == EDGE_FEATURE2 && 
			   f.y == (*datait)[j].label2order && 
			   (j > 0 && f.yp == (*datait)[j-1].label2order)) {
		    // edge feature (type 2)
		    gradlogli[f.idx] += f.val;
		    seq_logli += lambda[f.idx] * f.val;		    
		    		
		} else if (f.ftype == STAT_FEATURE1 && f.y == (*datait)[j].label) {
		    // state feature (type 1)
		    gradlogli[f.idx] += f.val;
		    seq_logli += lambda[f.idx] * f.val;		    
		
		} else if (f.ftype == STAT_FEATURE2 && f.y == (*datait)[j].label2order) {
		    // state feature (type 2)
		    gradlogli[f.idx] += f.val;
		    seq_logli += lambda[f.idx] * f.val;		    		    
		}

		if (f.ftype == EDGE_FEATURE1) {
		    // edge feature (type 1)    
		    int index = f.yp * popt->num_labels + f.y;
		    ExpF[f.idx] += (*next_alpha)[index] * f.val * (*(betas[j]))[index];
		
		} else if (f.ftype == EDGE_FEATURE2) {
		    // edge feature (type 2)
		    ExpF[f.idx] += (*alpha)[f.yp] * (*Vi)[f.y] * Mi->mtrx[f.yp][f.y] 
				    * f.val * (*(betas[j]))[f.y];
		
		} else if (f.ftype == STAT_FEATURE1) {
		    // state feature (type 1)
		    for (int i = 0; i < popt->num_labels; i++) {
			int index = i * popt->num_labels + f.y;
			ExpF[f.idx] += (*next_alpha)[index] * f.val * (*(betas[j]))[index];
		    }
		
		} else if (f.ftype == STAT_FEATURE2) {
		    // state feature (type 2)
		    ExpF[f.idx] += (*next_alpha)[f.y] * f.val * (*(betas[j]))[f.y];
		}
	    }	    
	    
	    *alpha = *next_alpha;
	    alpha->comp_mult(1.0 / scale[j]);	    
	} 

	// Zx = sum(alpha_i_n) where i = 1..num_labels, n = seq_len
	double Zx = alpha->sum();
	
	// Log-likelihood of the current sequence
	// seq_logli = lambda * F(y_k, x_k) - log(Zx_k)
	// where x_k is the current sequence
	seq_logli -= log(Zx);
	
	// re-correct the value of seq_logli because Zx was computed from
	// scaled alpha values
	for (k = 0; k < seq_len; k++) {
	    seq_logli -= log(scale[k]);
	}

	// Log-likelihood = sum_k[lambda * F(y_k, x_k) - log(Zx_k)]
	logli += seq_logli;
	
	// update the gradient vector
	for (k = 0; k < num_features; k++) {
	    gradlogli[k] -= ExpF[k] / Zx;
	}

    } // end of the main loop

    // output some status information
    if (popt->debug_level > 0) {
	printf("\n");
	printf("Iteration: %d\n", num_iters);
	printf("\tLog-likelihood                       = %17.6f\n", logli);
	double gradlogli_norm = trainer::norm(num_features, gradlogli);
	printf("\tNorm(log-likelihood gradient vector) = %17.6f\n", gradlogli_norm);		
	double lambda_norm = trainer::norm(num_features, lambda);    
	printf("\tNorm(lambda vector)                  = %17.6f\n", lambda_norm);
		
	if (is_logging) {
	    fprintf(fout, "\n");
	    fprintf(fout, "Iteration: %d\n", num_iters);
	    fprintf(fout, "\tLog-likelihood                       = %17.6f\n", logli);
	    fprintf(fout, "\tNorm(log-likelihood gradient vector) = %17.6f\n", gradlogli_norm);		
	    fprintf(fout, "\tNorm(lambda vector)                  = %17.6f\n", lambda_norm);
	}
    }    
    
    return logli;
}

// compute log Mi (second-order Markov)
void trainer::compute_log_Mi_2order(sequence & seq, int pos, doublematrix * Mi, 
		  doublevector * Vi, int is_exp) {
    *Mi = 0.0;
    *Vi = 0.0;

    // start scan features for sequence "seq" at position "i"
    pfgen->start_scan_features_at(seq, pos);
    // examine all features at position "pos"
    while (pfgen->has_next_feature()) {
	feature f;
	pfgen->next_feature(f);
	
	if (f.ftype == EDGE_FEATURE1) {
	    // edge feature (type 1)
	    (*Vi)[f.yp * popt->num_labels + f.y] += lambda[f.idx] * f.val;    
	
	} else if (f.ftype == EDGE_FEATURE2) {
	    // edge feature (type 2)
	    Mi->get(f.yp, f.y) += lambda[f.idx] * f.val;
	
	} else if (f.ftype == STAT_FEATURE1) {
	    // state feature (type 1)
	    for (int i = 0; i < popt->num_labels; i++) {
		(*Vi)[i * popt->num_labels + f.y] += lambda[f.idx] * f.val;
	    }
	
	} else if (f.ftype == STAT_FEATURE2) {
	    // state feature (type 2)
	    (*Vi)[f.y] += lambda[f.idx] * f.val;
	}
    }
    
    // take exponential operator
    if (is_exp) {
	for (int i = 0; i < Mi->rows; i++) {
	    // update for Vi
	    (*Vi)[i] = exp((*Vi)[i]);
	    // update for Mi
	    for (int j = 0; j < Mi->cols; j++) {
		Mi->get(i, j) = exp(Mi->get(i, j));
	    }
	}
    }
}

