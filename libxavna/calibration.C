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
    MatrixXcd computeCoefficients(const vector<VNARawValue>& measurements,
                                  const vector<VNACalibratedValue>& calStdModels) const override {                    
        // TODO(xaxaxa): actually use calStdModels rather than just assume ideal cal standards
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
    MatrixXcd computeCoefficients(const vector<VNARawValue>& measurements,
                                  const vector<VNACalibratedValue>& calStdModels) const override {
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
map<string, VNACalibratedValue> idealCalStds;


int initCalStds() {
    idealCalStds["short1"] << -1., 0,
                                0, -1.;
    idealCalStds["open1"] << 1., 0,
                            0, 1.;
    idealCalStds["load1"] << 0, 0,
                            0, 0;
    idealCalStds["thru"] << 0, 1.,
                            1., 0;
    idealCalStds["short2"] = idealCalStds["short1"];
    idealCalStds["open2"] = idealCalStds["open1"];
    idealCalStds["load2"] = idealCalStds["load1"];
    return 0;
}
int tmp = initCalStds();


// CalibrationEngine


CalibrationEngine::CalibrationEngine(int nPorts) {
    _nPorts = nPorts;
    _nEquations = 0;
    _equations = MatrixXcd::Zero(nCoeffs(), nCoeffs());
    _rhs = VectorXcd::Zero(nCoeffs());
}

int CalibrationEngine::nEquations() {
    return _nEquations;
}

int CalibrationEngine::nCoeffs() {
    return _nPorts*_nPorts*4;
}

int CalibrationEngine::nEquationsRequired() {
    return nCoeffs();
}

void CalibrationEngine::clearEquations() {
    _nEquations = 0;
}

#define T1(ROW, COL) equation(ROW*_nPorts + COL)
#define T2(ROW, COL) equation(TSize + ROW*_nPorts + COL)
#define T3(ROW, COL) equation(TSize*2 + ROW*_nPorts + COL)
#define T4(ROW, COL) equation(TSize*3 + ROW*_nPorts + COL)

void CalibrationEngine::addFullEquation(const MatrixXcd &actualSParams, const MatrixXcd &measuredSParams) {
    int TSize = _nPorts*_nPorts;
    const MatrixXcd& S = actualSParams;
    const MatrixXcd& M = measuredSParams;
    

    // T1*S + T2 - M*T3*S - M*T4 = 0
    // T1, T2, T3, T4 are unknowns
    // the rhs is a size nPort*nPort zero matrix

    // iterate through the entries of the rhs,
    // and add one equation for each entry
    for(int row=0;row<_nPorts;row++)
        for(int col=0;col<_nPorts;col++) {
            if(_nEquations >= nCoeffs()) throw logic_error("calibration engine: too many equations; required: " + to_string(nCoeffs()));
            VectorXcd equation = VectorXcd::Zero(nCoeffs());

            // + T1*S
            for(int i=0;i<_nPorts;i++) {
                T1(row, i) = S(i, col);
            }

            // + T2
            T2(row, col) = 1.;

            // - M*T3*S
            for(int i=0;i<_nPorts;i++)
                for(int j=0;j<_nPorts;j++) {
                    /* to derive these coefficients, play around in sympy:
                    from sympy import *
                    M = MatrixSymbol('M', 2, 2)
                    T = MatrixSymbol('T', 2, 2)
                    S = MatrixSymbol('S', 2, 2)
                    rhs = Matrix(M)*Matrix(T)*Matrix(S)

                    print expand(rhs[0,0])
                    print expand(rhs[0,1])
                    print expand(rhs[1,1])
                    */
                    T3(i, j) = -M(row,i)*S(j,col);
                }

            // - M*T4
            for(int i=0;i<_nPorts;i++) {
                T4(i,col) = -M(row,i);
            }

            _equations.row(_nEquations) = equation;
            _rhs(_nEquations) = 0.;
            _nEquations++;
        }
}

void CalibrationEngine::addEquation(const MatrixXcd &actualSParams, const MatrixXcd &measuredSParams, const MatrixXi &map) {

}

void CalibrationEngine::addNormalizingEquation() {
    int TSize = _nPorts*_nPorts;
    if(_nEquations >= nCoeffs()) throw logic_error("calibration engine: too many equations; required: " + to_string(nCoeffs()));
    VectorXcd equation = VectorXcd::Zero(nCoeffs());
    T3(0,0) = 1.;
    _equations.row(_nEquations) = equation;
    _rhs(_nEquations) = 1.;
    _nEquations++;
}

#undef T1
#undef T2
#undef T3
#undef T4

MatrixXcd CalibrationEngine::computeCoefficients() {
    auto tmp = _equations.colPivHouseholderQr();
    if(tmp.rank() != nCoeffs()) throw runtime_error("matrix rank is not full!");

    int TSize = _nPorts*_nPorts;

    VectorXcd res = tmp.solve(_rhs);
    MatrixXcd T = MatrixXcd::Zero(_nPorts*2, _nPorts*2);
    for(int i=0;i<4;i++)
        for(int row=0;row<_nPorts;row++)
            for(int col=0;col<_nPorts;col++) {
                int colOffset = int(i%2) * _nPorts;
                int rowOffset = int(i/2) * _nPorts;
                T(row+rowOffset, col+colOffset) = res(TSize*i + row*_nPorts + col);
            }
    return T;
}

MatrixXcd CalibrationEngine::computeSParams(const MatrixXcd &coeffs, const MatrixXcd &measuredSParams) {
    int _nPorts = measuredSParams.rows();
    MatrixXcd T1 = coeffs.topLeftCorner(_nPorts, _nPorts);
    MatrixXcd T2 = coeffs.topRightCorner(_nPorts, _nPorts);
    MatrixXcd T3 = coeffs.bottomLeftCorner(_nPorts, _nPorts);
    MatrixXcd T4 = coeffs.bottomRightCorner(_nPorts, _nPorts);
    MatrixXcd A = T1 - measuredSParams*T3;
    MatrixXcd rhs = measuredSParams*T4 - T2;
    return A.colPivHouseholderQr().solve(rhs);
}



}
