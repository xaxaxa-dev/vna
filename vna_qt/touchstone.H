#ifndef TOUCHSTONE_H
#define TOUCHSTONE_H
#include <string>
#include <vector>
#include <eigen3/Eigen/Core>
#include <xavna/common.H>
using namespace std;
using namespace Eigen;

string serializeTouchstone(vector<complex<double> > data, double startFreqHz, double stepFreqHz);
string serializeTouchstone(vector<Matrix2cd> data, double startFreqHz, double stepFreqHz);
void parseTouchstone(string data, double& startFreqHz, double& stepFreqHz, int &nPorts, vector<Matrix2cd>& results);

#endif // TOUCHSTONE_H
