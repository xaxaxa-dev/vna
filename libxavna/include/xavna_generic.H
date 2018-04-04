#include <functional>

using namespace std;
class xavna_generic {
public:
	virtual int set_params(int freq_khz, int atten1, int atten2)=0;
	virtual int read_values(double* out_values, int n_samples)=0;
	virtual int read_values_raw(double* out_values, int n_samples)=0;
	virtual ~xavna_generic() {}
};

typedef function<xavna_generic*(const char* dev)> xavna_constructor;


