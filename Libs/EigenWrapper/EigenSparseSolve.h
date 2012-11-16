#ifndef __EigenWrapper_h
#define __EigenWrapper_h

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include "BenderEigenWrapperExport.h"

typedef Eigen::SparseMatrix<float> SpMat;

//Solve a sparse matrix.  Just wrap around Eignen
Eigen::VectorXf BENDER_EIGENWRAPPER_EXPORT Solve(SpMat& A,  Eigen::VectorXf& b);

#endif
