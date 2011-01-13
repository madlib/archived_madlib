"""
@file logRegress.py

@brief Logistic Regression using conjugate gradient ascent

@namespace logRegress

Logistic Regression using conjugate gradient ascent

Logistic regression is used to estimate probabilities of a binary variable, by
fitting a stochastic model. It is one of the most commonly used tools for
applied statistics and data mining [1].

Literature:

[1] Cosma Shalizi, Statistics 36-350: Data Mining, Lecture Notes, 18 November
    2009, http://www.stat.cmu.edu/~cshalizi/350/lectures/26/lecture-26.pdf

[2] Thomas P. Minka, A comparison of numerical optimizers for logistic
    regression, 2003 (revised Mar 26, 2007),
    http://research.microsoft.com/en-us/um/people/minka/papers/logreg/minka-logreg.pdf

Logistic regression assumes a generalized linear model:
@verbatim
  E[Y] = g^{-1}(c^T X)

where:
Y     = the dependent variable
c^T X = the linear predictor
              x                                                       1
g(x)  = ln( ----- ) is the link function and σ(x) := g^{-1}(x) = -----------
            1 - x                                                1 + exp(-x)
@endverbatim
For each training data point $i$, we have a vector of
features $x_i$ and an observed class $y_i$. For ease of notation, let $Z$ be a
dependent random variable such that $Z = -1$ if $Y = 0$ and $Z = 1$ if $Y = 1$,
i.e., \f$Z := 2(Y - \frac 12)\f$. By definition,
\f$P[Z = z_i | X = x_i] = σ(z_i \cdot \boldsymbol c^T x_i)\f$.

Since logistic regression predicts probabilities, we can do maximum-likelihood
fitting: That is, we want the vector of regression coefficients
\f$\boldsymbol c\f$ to maximize
@verbatim
  __n
  ||    σ(z_i c^T x_i)
    i=1
@endverbatim
or, equivalently, to maximize the objective
@verbatim
            n
           --
  l(c) = - \  log(1 + exp(-z_i c^T x_i))
           /_
           i=1
@endverbatim
By looking at the Hessian, we can verify that \f$l(\boldsymbol c)\f$ is convex.


\par Maximizing the objective with the conjugate gradient method

The method we are using is known as Fletcher–Reeves method in the literature,
where we use the Hestenes-Stiefel rule for calculating the step size.

The gradient of \f$l(\boldsymbol c)\f$ is
@verbatim
              n
             --  
  ∇_c l(c) = \  (1 - σ(z_i c^T x_i)) z_i x_i
             /_ 
             i=1
@endverbatim

We compute
@verbatim
For k = 0, 1, 2, ...:

                    n
                   --  
  g_0 = ∇_c l(0) = \  (1 - σ(z_i c^T x_i)) z_i x_i
                   /_ 
                   i=1
  
  d_0 = g_0
  
         g_0^T d_0
  c_0 = ----------- d_0
        d_0^T H d_0
    
For k = 1, 2, ...:

  g_k = ∇_c l(c_{k-1})
  
         g_k^T (g_k - g_{k-1})
  β_k = -----------------------
        d_{k-1} (g_k - g_{k-1})

  d_k = g_k - β_k d_{k-1}
  
                   g_k^T d_k
  c_k = c_{k-1} + ----------- d_k
                  d_k^T H d_k

where:
                   n
                  --
  d_k^T H d_k = - \  σ(c^T x_i) (1 - σ(c^T x_i)) (d^T x_i)^2 
                  /_
                  i=1

and

H = the Hessian of the objective
@endverbatim
"""

import plpy
        
