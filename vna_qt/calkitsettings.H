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
};
#endif // CALKITSETTINGS_H
