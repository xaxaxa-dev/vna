#include "include/calibration.H"
namespace xaxaxa {
class SOLCalibration: public VNACalibration {
public:
	// return the name of this calibration type
    string name() const override {
		return "SOL (1 port)";
	}
	// get a list of calibration standards required
    vector<string> getRequiredStandards() const override {
		return {"short1,Short","open1,Open","load1,Load"};
	}
	// given the measurements for each of the calibration standards, compute the coefficients
    MatrixXcd computeCoefficients(const vector<VNARawValue>& measurements) const override {
		auto tmp = SOL_compute_coefficients(measurements[0](0,0), measurements[1](0,0), measurements[2](0,0));
		Vector3cd ret(tmp[0],tmp[1],tmp[2]);
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

class SOLTCalibrationTR: public VNACalibration {
public:
	// return the name of this calibration type
    string name() const override {
		return "SOLT (T/R)";
	}
	// get a list of calibration standards required
    vector<string> getRequiredStandards() const override {
		return {"short1,Short","open1,Open","load1,Load","thru,Thru"};
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

class SOLTCalibration: public VNACalibration {
public:
	// return the name of this calibration type
    string name() const override {
		return "SOLT (two port)";
	}
	// get a list of calibration standards required
    vector<string> getRequiredStandards() const override {
		return {"short1,Short (port 1)", "short2,Short (port 2)",
				"open1,Open (port 1)", "open2,Open (port 2)",
				"load1,Load (port 1)", "load2,Load (port 2)",
				"thru,Thru"};
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
	return {new SOLCalibration(), new SOLTCalibrationTR(), new SOLTCalibration()};
}


vector<const VNACalibration*> calibrationTypes = initCalTypes();

}
