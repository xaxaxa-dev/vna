#ifndef UTILITY_H
#define UTILITY_H
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <string>
#include <QString>
using namespace std;

inline QString qs(const string& s) {
    return QString::fromStdString(s);
}
inline double dB(double power) {
    return log10(power)*10;
}
// freq is in Hz, Z is in ohms
inline double capacitance_inductance(double freq, double Z) {
    if(Z>0) return Z/(2*M_PI*freq);
    return 1./(2*Z*M_PI*freq);
}
// freq is in Hz, Y is in mhos
inline double capacitance_inductance_Y(double freq, double Y) {
    if(Y<0) return -1./(2*Y*M_PI*freq);
    return -Y/(2*M_PI*freq);
}
inline double si_scale(double val) {
    double val2 = fabs(val);
    if(val2>1e12) return val*1e-12;
    if(val2>1e9) return val*1e-9;
    if(val2>1e6) return val*1e-6;
    if(val2>1e3) return val*1e-3;
    if(val2>1e0) return val;
    if(val2>1e-3) return val*1e3;
    if(val2>1e-6) return val*1e6;
    if(val2>1e-9) return val*1e9;
    if(val2>1e-12) return val*1e12;
    return val*1e15;
}
inline const char* si_unit(double val) {
    val = fabs(val);
    if(val>1e12) return "T";
    if(val>1e9) return "G";
    if(val>1e6) return "M";
    if(val>1e3) return "k";
    if(val>1e0) return "";
    if(val>1e-3) return "m";
    if(val>1e-6) return "u";
    if(val>1e-9) return "n";
    if(val>1e-12) return "p";
    return "f";
}
inline string ssprintf(int maxLen, const char* fmt, ...) {
    string tmp(maxLen, '\0');
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf((char*)tmp.data(), maxLen, fmt, args);
    va_end(args);
    tmp.resize(len);
    return tmp;
}


#endif // UTILITY_H
