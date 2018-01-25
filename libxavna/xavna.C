#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <poll.h>

#include <complex>
#include <tuple>

#include "common_types.h"
#include "include/xavna.h"
using namespace std;

extern "C" {
	int nWait=50;		//number of data points to skip after changing frequency
	
	struct complex5 {
		complex<double> val[5];
	};

	// the structure pointed to by the device handle
	struct xavna_device {
		int ttyFD;
	};
	
	static tuple<complex5,int> readValue3(int ttyFD, int cnt);
	
		
	static void drainfd(int fd) {
		pollfd pfd;
		pfd.fd = fd;
		pfd.events = POLLIN;
		while(poll(&pfd,1,0)>0) {
			if(!(pfd.revents&POLLIN)) continue;
			char buf[4096];
			read(fd,buf,sizeof(buf));
		}
	}
	

	// dev: path to the serial device; if NULL, it will be selected automatically
	// returns: handle to the opened device, or NULL if failed; check errno
	void* xavna_open(const char* dev) {
		xavna_device* d = new xavna_device();
		d->ttyFD=open(dev,O_RDWR);
		if(d->ttyFD<0) {
			delete d;
			return NULL;
		}
		struct termios tc;
		/* Set TTY mode. */
		if (tcgetattr(d->ttyFD, &tc) < 0) {
			perror("tcgetattr");
			goto skip_tcsetaddr;
		}
		tc.c_iflag &= ~(INLCR|IGNCR|ICRNL|IGNBRK|IUCLC|INPCK|ISTRIP|IXON|IXOFF|IXANY);
		tc.c_oflag &= ~OPOST;
		tc.c_cflag &= ~(CSIZE|CSTOPB|PARENB|PARODD|CRTSCTS);
		tc.c_cflag |= CS8 | CREAD | CLOCAL;
		tc.c_lflag &= ~(ICANON|ECHO|ECHOE|ECHOK|ECHONL|ISIG|IEXTEN);
		tc.c_cc[VMIN] = 1;
		tc.c_cc[VTIME] = 0;
		tcsetattr(d->ttyFD, TCSANOW, &tc);
	skip_tcsetaddr:
		return d;
	}

	void* xavna_get_chained_device(void* dev) {
		return NULL;
	}

	// Set the RF frequency.
	// freq_khz: frequency in kHz
	// returns: actual frequency set, in kHz; -1 if failure
	int xavna_set_frequency(void* dev, int freq_khz) {
		xavna_device* d = (xavna_device*)dev;
		int attenuation=0;
		double freq = double(freq_khz)/1000.;
		int N = (int)round(freq*100);
		
		//attenuation = 20;
		
		if(freq<1000) attenuation=13;
		else if(freq<1500) attenuation=10;
		else if(freq<1730) attenuation=7;
		else if(freq<2130) attenuation=6;
		else if(freq<2900) attenuation=2;
		else attenuation=0;
		
		//attenuation += 18;
		//attenuation += 5;
		attenuation = 20;
		
		/*
		if(freq<400) attenuation=20;
		else if(freq<1000) attenuation=18;
		else if(freq<2000) attenuation=14;
		else if(freq<3000) attenuation=8;
		else attenuation=4;*/
		
		u8 buf[] = {
			1, u8(N>>16),
			2, u8(N>>8),
			3, u8(N),
			5, u8(attenuation*2),
			6, u8(0b00001100),
			0, 0,
			4, 1
		};
		assert(write(d->ttyFD,buf,sizeof(buf))==(int)sizeof(buf));
		
		drainfd(d->ttyFD);
		readValue3(d->ttyFD, nWait);
		return N*10;
	}

	// Set the signal generator attenuation
	// a: attenuation in dB
	// returns: 0 if success; -1 if failure
	int xavna_set_attenuation(void* dev, int a) {
		return 0;
	}

	// Set whether the signal source is enabled.
	// en: 0 to disable, nonzero to enable
	// returns: 0 if success, -1 if failure
	int xavna_set_source_enabled(void* dev, int en) {
		return 0;
	}


	// out_values: array of size 4 holding the following values:
	//				reflection real, reflection imag,
	//				thru real, thru imag
	// n_samples: number of samples to average over; typical 50
	// returns: number of samples read, or -1 if failure
	int xavna_read_values(void* dev, double* out_values, int n_samples) {
		xavna_device* d = (xavna_device*)dev;
		complex5 result;
		int n;
		tie(result, n) = readValue3(d->ttyFD, n_samples);
		out_values[0] = result.val[3].real();
		out_values[1] = result.val[3].imag();
		out_values[2] = result.val[4].real();
		out_values[3] = result.val[4].imag();
		return n;
	}
	
	// out_values: array of size 4 holding the following values:
	//				reflection real, reflection imag,
	//				thru real, thru imag
	// n_samples: number of samples to average over; typical 50
	// returns: number of samples read, or -1 if failure
	int xavna_read_values_raw(void* dev, double* out_values, int n_samples) {
		xavna_device* d = (xavna_device*)dev;
		double scale = 1.d/double((int64_t(1)<<12) * (int64_t(1)<<19));
		complex5 result;
		int n;
		tie(result, n) = readValue3(d->ttyFD, n_samples);
		
		complex<double> refl = result.val[1], thru = result.val[0];
		complex<double> reference = polar(1., -arg(result.val[0]));
		refl *= reference;
		thru *= reference;
		
		out_values[0] = refl.real()*scale;
		out_values[1] = refl.imag()*scale;
		out_values[2] = thru.real()*scale;
		out_values[3] = thru.imag()*scale;
		return n;
	}
	
	// out_values: array of size 10 holding the following values:
	//				reference real, reference imag,
	//				raw reflection real, raw reflection imag,
	//				raw thru real, raw thru imag
	//				reflection real, reflection imag,
	//				thru real, thru imag
	// n_samples: number of samples to average over; typical 50
	// returns: number of samples read, or -1 if failure
	int xavna_read_values_raw2(void* dev, double* out_values, int n_samples) {
		xavna_device* d = (xavna_device*)dev;
		complex5 result;
		int n;
		tie(result, n) = readValue3(d->ttyFD, n_samples);
		double scale = 1.d/double((int64_t(1)<<12) * (int64_t(1)<<19));
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

	// close device handle
	void xavna_close(void* dev) {
		xavna_device* d = (xavna_device*)dev;
		close(d->ttyFD);
		delete d;
	}



	
	u64 sx(u64 data, int bits) {
		u64 mask=~u64(u64(1LL<<bits) - 1);
		return data|((data>>(bits-1))?mask:0);
	}
	complex<double> processValue(u64 data1,u64 data2) {
		data1=sx(data1,35);
		data2=sx(data2,35);
		
		return {double((ll)data2), double((ll)data1)};
	}
	// returns [adc0, adc1, adc2, adc1/adc0, adc2/adc0]
	static tuple<complex5,int> readValue3(int ttyFD, int cnt) {
		complex5 result=complex5{{0,0,0,0,0}};
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
						result.val[3] += result.val[1]/result.val[0];
						result.val[4] += result.val[2]/result.val[0];
						if(++n >= cnt) {
							for(int g=0;g<5;g++)
								result.val[g] /= cnt;
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

}
