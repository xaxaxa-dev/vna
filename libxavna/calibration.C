#include "include/calibration.H"

typedef Matrix<complex<double>,5,1> Vector5cd;
typedef Matrix<complex<double>,6,1> Vector6cd;

namespace xaxaxa {
// T/R SOLT calibration and one port SOL calibration
class SOLTCalibrationTR: public VNACalibration {
public:
	bool noThru;
	SOLTCalibrationTR(bool noThru=false): noThru(noThru) {
		
	}
	// return the name of this calibration type
	virtual string name() const override {
		return noThru?"sol_1":"solt_tr";
	}
    string description() const override {
		return noThru?"SOL (1 port)":"SOLT (T/R)";
	}
	// get a list of calibration standards required
    vector<array<string, 2> > getRequiredStandards() const override {
		if(noThru) return {{"short1","Short"},{"open1","Open"},{"load1","Load"}};
		else return {{"short1","Short"},{"open1","Open"},{"load1","Load"},{"thru","Thru"}};
	}
	// given the measurements for each of the calibration standards, compute the coefficients
    MatrixXcd computeCoefficients(const vector<VNARawValue>& measurements) const override {
		auto tmp = SOL_compute_coefficients(measurements.at(0)(0,0), measurements.at(1)(0,0), measurements.at(2)(0,0));
		
		auto x1 = measurements[2](0,0),
			y1 = measurements[2](1,0),
			x2 = measurements[1](0,0),
			y2 = measurements[1](1,0);
		complex<double> cal_thru_leak_r = (y1-y2)/(x1-x2);
		complex<double> cal_thru_leak = y2-cal_thru_leak_r*x2;
		
		complex<double> thru = 1.;
		if(!noThru) thru = measurements.at(3)(1,0);
		
		Vector6cd ret;
		ret << tmp[0],tmp[1],tmp[2], cal_thru_leak, cal_thru_leak_r, thru;
		return ret;
	}
	// given cal coefficients and a raw value, compute S parameters
    VNACalibratedValue computeValue(MatrixXcd coeffs, VNARawValue val) const override {
        array<complex<double>, 3> tmp {coeffs(0), coeffs(1), coeffs(2)};
		VNACalibratedValue ret = val;
		ret(0,0) = SOL_compute_reflection(tmp, val(0,0));
		
		complex<double> cal_thru_leak_r = coeffs(4);
		complex<double> cal_thru_leak = coeffs(3);
		
		complex<double> thru = val(1,0) - (cal_thru_leak + val(0,0)*cal_thru_leak_r);
		complex<double> refThru = coeffs(5) - (cal_thru_leak + val(0,0)*cal_thru_leak_r);
		
		ret(1,0) = thru/refThru;
		return ret;
	}
};


class SOLTCalibration: public VNACalibration {
public:
	// return the name of this calibration type
	virtual string name() const override {
		return "solt";
	}
    string description() const override {
		return "SOLT (two port)";
	}
	// get a list of calibration standards required
    vector<array<string, 2> > getRequiredStandards() const override {
		return {{"short1","Short (port 1)"}, {"short2","Short (port 2)"},
				{"open1","Open (port 1)"}, {"open2","Open (port 2)"},
				{"load1","Load (port 1)"}, {"load2","Load (port 2)"},
				{"thru","Thru"}};
	}
	// given the measurements for each of the calibration standards, compute the coefficients
    MatrixXcd computeCoefficients(const vector<VNARawValue>& measurements) const override {
		auto tmp = SOL_compute_coefficients(measurements[0](0,0), measurements[1](0,0), measurements[2](0,0));
		Vector4cd ret(tmp[0],tmp[1],tmp[2],measurements[3](1,0));
		return ret;
	}
	// given cal coefficients and a raw value, compute S parameters
    VNACalibratedValue computeValue(MatrixXcd coeffs, VNARawValue val) const override {
        array<complex<double>, 3> tmp {coeffs(0), coeffs(1), coeffs(2)};
		VNACalibratedValue ret = val;
		ret(0,0) = SOL_compute_reflection(tmp, val(0,0));
		return ret;
	}
};

vector<const VNACalibration*> initCalTypes() {
	return {new SOLTCalibrationTR(true), new SOLTCalibrationTR(false), new SOLTCalibration()};
}


vector<const VNACalibration*> calibrationTypes = initCalTypes();

}
