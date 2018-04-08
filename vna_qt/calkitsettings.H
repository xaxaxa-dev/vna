#ifndef CALKITSETTINGS_H
#define CALKITSETTINGS_H
#include <map>
#include <string>
#include <vector>
#include <xavna/common.H>
using namespace std;
using namespace xaxaxa;

struct SParamSeries {
    // number of points is assumed from the length of values
    vector<VNACalibratedValue> values;
    double startFreqHz, stepFreqHz;
    VNACalibratedValue interpolate(double freqHz) {
        assert(!values.empty());
        int nPoints = (int)values.size();
        double index = (freqHz-startFreqHz)/stepFreqHz;
        int i1 = (int) floor(index);
        int i2 = i1+1;
        if(i1 < 0) return values[0];
        if(i2 >= nPoints) return values[nPoints-1];
        return values[i1]*(i2-index) + values[i2]*(index-i1);
    }
};
// the in-memory structure that holds the calibration kit settings
struct CalKitSettings {
    // if any given cal kit type is not present here, it should be assumed
    // to use ideal parameters
    map<string, SParamSeries> calKitModels;
    map<string, string> calKitNames;
};
#ifdef Q_DECLARE_METATYPE
Q_DECLARE_METATYPE(CalKitSettings);
#endif

class QDataStream;

QDataStream &operator<<(QDataStream &out, const complex<double> &myObj);
QDataStream &operator>>(QDataStream &in, complex<double> &myObj);

QDataStream &operator<<(QDataStream &out, const string &myObj);
QDataStream &operator>>(QDataStream &in, string &myObj);

QDataStream &operator<<(QDataStream &out, const Matrix2cd &myObj);
QDataStream &operator>>(QDataStream &in, Matrix2cd &myObj);

QDataStream &operator<<(QDataStream &out, const SParamSeries &myObj);
QDataStream &operator>>(QDataStream &in, SParamSeries &myObj);

QDataStream &operator<<(QDataStream &out, const CalKitSettings &myObj);
QDataStream &operator>>(QDataStream &in, CalKitSettings &myObj);

#endif // CALKITSETTINGS_H
