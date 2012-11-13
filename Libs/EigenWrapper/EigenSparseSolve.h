#ifndef __EigenWrapper_h
#define __EigenWrapper_h

#include <Eigen/Dense>
#include <Eigen/Sparse>

typedef Eigen::SparseMatrix<float> SpMat;

Eigen::VectorXf Solve(SpMat& A,  Eigen::VectorXf& b);

#endif
