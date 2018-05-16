#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <complex>
#include <tuple>
#include <map>
#include <functional>

#include "include/xavna_generic.H"
#include "common_types.h"
#include "include/xavna.h"
#include "include/platform_abstraction.H"
using namespace std;


// named virtual devices
map<string, xavna_constructor> xavna_virtual_devices;
xavna_constructor xavna_default_constructor;
extern "C" {
	int nWait=50;		//number of data points to skip after changing frequency
}


struct complex5 {
	complex<double> val[5];
};

static u64 sx(u64 data, int bits) {
	u64 mask=~u64(u64(1LL<<bits) - 1);
	return data|((data>>(bits-1))?mask:0);
}
static complex<double> processValue(u64 data1,u64 data2) {
	data1=sx(data1,35);
	data2=sx(data2,35);
	
	return {double((ll)data2), double((ll)data1)};
}
// returns [adc0, adc1, adc2, adc1/adc0, adc2/adc0]
static tuple<complex5,int> readValue3(int ttyFD, int cnt) {
	complex5 result=complex5{{0.,0.,0.,0.,0.}};
	int bufsize=1024;
	u8 buf[bufsize];
	int br;
	u64 values[7];
	int n=0;	// how many sample groups we've read so far
	int j=0;	// bit offset into current value
	int k=0;	// which value in the sample group we are currently expecting
	
	u8 msb=1<<7;
	u8 mask=msb-1;
	u8 checksum = 0;
	while((br=read(ttyFD,buf,sizeof(buf)))>0) {
		for(int i=0;i<br;i++) {
			if((buf[i]&msb)==0) {
				if(k == 6 && j == 7) {
					checksum &= 0b1111111;
					if(checksum != u8(values[6])) {
						printf("ERROR: checksum should be %d, is %d\n", (int)checksum, (int)(u8)values[6]);
					}
					for(int g=0;g<3;g++)
						result.val[g] += processValue(values[g*2],values[g*2+1]);
					//result.val[3] += result.val[1]/result.val[0];
					//result.val[4] += result.val[2]/result.val[0];
					if(++n >= cnt) {
						for(int g=0;g<5;g++)
							result.val[g] /= cnt;
						result.val[3] = result.val[1]/result.val[0];
						result.val[4] = result.val[2]/result.val[0];
						return make_tuple(result, n);
					}
				}
				values[0]=values[1]=values[2]=0;
				values[3]=values[4]=values[5]=0;
				values[6]=0;
				j=0;
				k=0;
				checksum=0b01000110;
			}
			if(k<6) checksum = (checksum xor ((checksum<<1) | 1)) xor buf[i];
			if(k < 7)
				values[k] |= u64(buf[i]&mask) << j;
			j+=7;
			if(j>=35) {
				j=0;
				k++;
			}
		}
	}
	return make_tuple(result, n);
}


class xavna_default: public xavna_generic {
public:
	int ttyFD;
	xavna_default(const char* dev) {
		ttyFD=xavna_open_serial(dev);
		if(ttyFD < 0) {
			throw runtime_error(strerror(errno));
		}
	}
	virtual int set_params(int freq_khz, int atten1, int atten2) {
		int attenuation=atten1;
		double freq = double(freq_khz)/1000.;
		int N = (int)round(freq*100);
		
		int txpower = 0b11;
		int minAtten = 5;
		
		if(attenuation >= 9+minAtten) {
			txpower = 0b00;
			attenuation -= 9;
		} else if(attenuation >= 6+minAtten) {
			txpower = 0b01;
			attenuation -= 6;
		} else if(attenuation >= 3+minAtten) {
			txpower = 0b10;
			attenuation -= 3;
		}
		
		if(attenuation > 31) attenuation = 31;
		
		u8 buf[] = {
			1, u8(N>>16),
			2, u8(N>>8),
			3, u8(N),
			5, u8(attenuation*2),
			6, u8(0b00001100 | txpower),
			0, 0,
			4, 1
		};
		if(write(ttyFD,buf,sizeof(buf))!=(int)sizeof(buf)) return -1;
		
		xavna_drainfd(ttyFD);
		readValue3(ttyFD, nWait);
		return 0;
	}

	virtual int read_values(double* out_values, int n_samples) {
		complex5 result;
		int n;
		tie(result, n) = readValue3(ttyFD, n_samples);
		out_values[0] = result.val[3].real();
		out_values[1] = result.val[3].imag();
		out_values[2] = result.val[4].real();
		out_values[3] = result.val[4].imag();
		return n;
	}
	
	virtual int read_values_raw(double* out_values, int n_samples) {
		complex5 result;
		int n;
		tie(result, n) = readValue3(ttyFD, n_samples);
		out_values[0] = result.val[0].real();
		out_values[1] = result.val[0].imag();
		out_values[2] = result.val[1].real();
		out_values[3] = result.val[1].imag();
		out_values[4] = 0.;
		out_values[5] = 0.;
		out_values[6] = result.val[2].real();
		out_values[7] = result.val[2].imag();
		return n;
	}
	
	virtual int read_values_raw2(double* out_values, int n_samples) {
		complex5 result;
		int n;
		tie(result, n) = readValue3(ttyFD, n_samples);
		double scale = 1./double((int64_t(1)<<12) * (int64_t(1)<<19));
		result.val[0] *= scale;
		result.val[1] *= scale;
		result.val[2] *= scale;
		
		//FIXME: make hardware phase consistent (enable PLL phase resync)
		// and remove this code
		complex<double> refPhase = polar(1., -arg(result.val[0]));
		result.val[0] *= refPhase;
		result.val[1] *= refPhase;
		result.val[2] *= refPhase;
		
		out_values[0] = result.val[0].real();
		out_values[1] = result.val[0].imag();
		out_values[2] = result.val[1].real();
		out_values[3] = result.val[1].imag();
		out_values[4] = result.val[2].real();
		out_values[5] = result.val[2].imag();
		out_values[6] = result.val[3].real();
		out_values[7] = result.val[3].imag();
		out_values[8] = result.val[4].real();
		out_values[9] = result.val[4].imag();
		return n;
	}

	virtual ~xavna_default() {
		close(ttyFD);
	}
};

static int __init_xavna_default() {
	xavna_default_constructor = [](const char* dev){ return new xavna_default(dev); };
	return 0;
}

static int ghsfkghfjkgfs = __init_xavna_default();



extern "C" {
	void* xavna_open(const char* dev) {
		auto it = xavna_virtual_devices.find(dev);
		if(it != xavna_virtual_devices.end()) return (*it).second(dev);
		try {
			return xavna_default_constructor(dev);
		} catch(exception& ex) {
			return NULL;
		}
	}

	int xavna_set_params(void* dev, int freq_khz, int atten1, int atten2) {
		return ((xavna_generic*)dev)->set_params(freq_khz, atten1, atten2);
	}

	int xavna_read_values(void* dev, double* out_values, int n_samples) {
		return ((xavna_generic*)dev)->read_values(out_values, n_samples);
	}
	
	int xavna_read_values_raw(void* dev, double* out_values, int n_samples) {
		return ((xavna_generic*)dev)->read_values_raw(out_values, n_samples);
	}
	
	int xavna_read_values_raw2(void* dev, double* out_values, int n_samples) {
		xavna_generic* tmp = (xavna_generic*)dev;
		return dynamic_cast<xavna_default*>(tmp)->read_values_raw2(out_values, n_samples);
	}

	void xavna_close(void* dev) {
		delete ((xavna_generic*)dev);
	}
}

