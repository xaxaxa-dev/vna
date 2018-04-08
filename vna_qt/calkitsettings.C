#include "calkitsettings.H"
#include <QDataStream>

QDataStream &operator<<(QDataStream &out, const complex<double> &myObj) {
    const double* val = reinterpret_cast<const double*>(&myObj);
    out << val[0];
    out << val[1];
    return out;
}

QDataStream &operator>>(QDataStream &in, complex<double> &myObj) {
    double* val = reinterpret_cast<double*>(&myObj);
    in >> val[0];
    in >> val[1];
    return in;
}


QDataStream &operator<<(QDataStream &out, const string &myObj) {
    int sz = (int)myObj.length();
    out << sz;
    out.writeRawData(myObj.data(), sz);
    return out;
}

QDataStream &operator>>(QDataStream &in, string &myObj) {
    int sz = 0;
    in >> sz;
    myObj.resize(sz);
    if(in.readRawData(&myObj[0], sz) != sz)
        throw runtime_error("short read from QDataStream");
    return in;
}


QDataStream &operator<<(QDataStream &out, const Matrix2cd &myObj) {
    for(int i=0; i<(myObj.cols()*myObj.rows()); i++)
        out << myObj(i);
    return out;
}

QDataStream &operator>>(QDataStream &in, Matrix2cd &myObj) {
    for(int i=0; i<(myObj.cols()*myObj.rows()); i++)
        in >> myObj(i);
    return in;
}

QDataStream &operator<<(QDataStream &out, const SParamSeries &myObj) {
    out << myObj.startFreqHz;
    out << myObj.stepFreqHz;
    out << (int)myObj.values.size();
    for(int i=0; i<(int)myObj.values.size(); i++)
        out << myObj.values.at(i);
    return out;
}

QDataStream &operator>>(QDataStream &in, SParamSeries &myObj) {
    int n=0;
    in >> myObj.startFreqHz;
    in >> myObj.stepFreqHz;
    in >> n;
    myObj.values.resize(n);
    for(int i=0; i<n; i++)
        in >> myObj.values.at(i);
    return in;
}

template<class S,class T>
QDataStream &operator<<(QDataStream &out, const map<S,T> &m) {
    int sz = m.size();
    out << sz;
    for(auto it=m.begin(); it!=m.end(); it++) {
        out << (*it).first;
        out << (*it).second;
    }
    return out;
}
template<class S,class T>
QDataStream &operator>>(QDataStream &in, map<S,T> &m) {
    int sz = 0;
    in >> sz;
    m.clear();
    for(int i=0;i<sz;i++) {
        S key;
        in >> key;
        in >> m[key];
    }
    return in;
}

QDataStream &operator<<(QDataStream &out, const CalKitSettings &myObj) {
    out << myObj.calKitModels;
    out << myObj.calKitNames;
    return out;
}

QDataStream &operator>>(QDataStream &in, CalKitSettings &myObj) {
    in >> myObj.calKitModels;
    in >> myObj.calKitNames;
    return in;
}



