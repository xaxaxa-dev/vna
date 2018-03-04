#define _USE_MATH_DEFINES
#include "touchstone.H"
#include <stdarg.h>
#include <stdio.h>
#include <math.h>
#if __cplusplus < 201103L
  #error This library needs at least a C++11 compliant compiler
#endif

// append to dst
int saprintf(string& dst, const char* fmt, ...) {
    int bytesToAllocate=32;
    int originalLen=dst.length();
    while(true) {
        dst.resize(originalLen+bytesToAllocate);
        va_list args;
        va_start(args, fmt);
        // ONLY WORKS WITH C++11!!!!!!!!
        // .data() does not guarantee enough space for the null byte before c++11
        int len = vsnprintf((char*)dst.data()+originalLen, bytesToAllocate+1, fmt, args);
        va_end(args);
        if(len>=0 && len <= bytesToAllocate) {
            dst.resize(originalLen+len);
            return len;
        }
        if(len<=0) bytesToAllocate*=2;
        else bytesToAllocate = len;
    }
}

string serializeTouchstone(vector<complex<double> > data, double startFreqHz, double stepFreqHz) {
    string res;
    res += "# MHz S MA R 50\n";
    saprintf(res,"!   freq        S11       \n");
    for(int i=0;i<(int)data.size();i++) {
        double freqHz = startFreqHz + i*stepFreqHz;
        double freqMHz = freqHz*1e-6;
        complex<double> val = data[i];
        double c = 180./M_PI;
        saprintf(res,"%8.3f %8.5f %7.2f\n",
                 freqMHz, abs(val), arg(val)*c);
    }
    return res;
}

string serializeTouchstone(vector<Matrix2cd> data, double startFreqHz, double stepFreqHz) {
    string res;
    res += "# MHz S MA R 50\n";
    saprintf(res,"!   freq        S11              S21              S12              S22       \n");
    for(int i=0;i<(int)data.size();i++) {
        double freqHz = startFreqHz + i*stepFreqHz;
        double freqMHz = freqHz*1e-6;
        Matrix2cd val = data[i];
        double c = 180./M_PI;
        saprintf(res,"%8.3f %8.5f %7.2f %8.5f %7.2f %8.5f %7.2f %8.5f %7.2f\n",
                 freqMHz,
                 abs(val(0,0)), arg(val(0,0))*c,
                 abs(val(1,0)), arg(val(1,0))*c,
                 abs(val(0,1)), arg(val(0,1))*c,
                 abs(val(1,1)), arg(val(1,1))*c);
    }
    return res;
}

