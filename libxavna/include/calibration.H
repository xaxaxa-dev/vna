#include <array>
#include <complex>
using namespace std;

// given the measured raw values for short, open, and load, compute the 3 calibration coefficients
inline array<complex<double>, 3> SOL_compute_coefficients(complex<double> sc, complex<double> oc, complex<double> load) {
	complex<double> a=load, b=oc, c=sc;
	complex<double> cal_X, cal_Y, cal_Z;
	cal_Z=(2.d*a-b-c)/(b-c);
	cal_X=a-c*(1.d-cal_Z);
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

