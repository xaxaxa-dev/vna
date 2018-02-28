#ifndef __XAVNA_COMMON_H
#define __XAVNA_COMMON_H
#include <eigen3/Eigen/Dense>
#include <eigen3/Eigen/StdVector>
#include <vector>
using namespace Eigen;
using namespace std;


namespace xaxaxa {
using VNARawValue = Matrix2cd;
using VNACalibratedValue = Matrix2cd;
}


EIGEN_DEFINE_STL_VECTOR_SPECIALIZATION(Matrix2cd);
#endif
