#ifndef __XAVNA_CALIBRATION_H
#define __XAVNA_CALIBRATION_H
#include <array>
#include <complex>
#include <eigen3/Eigen/Dense>
#include <vector>
#include "common.H"
using namespace std;
using namespace Eigen;

// given the measured raw values for short, open, and load, compute the 3 calibration coefficients
inline array<complex<double>, 3> SOL_compute_coefficients(complex<double> sc, complex<double> oc, complex<double> load) {
	complex<double> a=load, b=oc, c=sc;
	complex<double> cal_X, cal_Y, cal_Z;
	cal_Z=(2.*a-b-c)/(b-c);
	cal_X=a-c*(1.-cal_Z);
	cal_Y=a/cal_X;
	
	return {cal_X, cal_Y, cal_Z};
}
// given the calibration coefficients and a raw value, compute the reflection coefficient
inline complex<double> SOL_compute_reflection(const array<complex<double>, 3>& coeffs, complex<double> raw) {
	auto cal_X = coeffs[0];
	auto cal_Y = coeffs[1];
	auto cal_Z = coeffs[2];
	
	return (cal_X*cal_Y-raw)/(raw*cal_Z-cal_X);
}

// given the cal coefficients and a reflection coefficient, compute d(refl)/d(raw)
inline complex<double> SOL_compute_sensitivity(const array<complex<double>, 3>& coeffs, complex<double> refl) {
	auto X = coeffs[0];
	auto Y = coeffs[1];
	auto Z = coeffs[2];
	auto raw = X*(Y+refl)/(refl*Z+1.);
	return -X*(Y*Z-1.) / ((Z*raw-X)*(Z*raw-X));
}

// S: s11, s12, s21, s22; refl: reflection coefficient
// computes reflection coefficient looking into port 1 of S, with port 2 of S connected to a load refl
inline complex<double> cascade_reflection(const array<complex<double>, 4>& S, complex<double> refl) {
	complex<double> s11=S[0];
	complex<double> s12=S[1];
	complex<double> s21=S[2];
	complex<double> s22=S[3];
	return s11 + s12*s21*refl/(1.-s22*refl);
}

namespace xaxaxa {

class VNACalibration
{
public:
	// return the name of this calibration type
	virtual string name() const=0;
	// get a list of calibration standards required
	virtual vector<array<string, 2> > getRequiredStandards() const=0;
	// given the measurements for each of the calibration standards, compute the coefficients
	virtual MatrixXcd computeCoefficients(const vector<VNARawValue>& measurements) const=0;
	// given cal coefficients and a raw value, compute S parameters
	virtual VNACalibratedValue computeValue(MatrixXcd coeffs, VNARawValue val) const=0;
};

// system global list of calibration types (auto-populated on library init)
extern vector<const VNACalibration*> calibrationTypes;

}
#endif
