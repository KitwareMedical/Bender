#include "EigenSparseSolve.h"

Eigen::VectorXf Solve(SpMat& A,  Eigen::VectorXf& b)
{
  Eigen::SimplicialCholesky<SpMat> solver(A);  // performs a Cholesky factorization of A
  return solver.solve(b);
}
