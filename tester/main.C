#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>

#include <array>
#include <vector>
#include <complex>
#include <string>

#include <xavna/xavna.h>
#include <xavna/calibration.H>

#include "ui.H"

using namespace std;

int rfSwitchPins[3] = {7,8,9};
constexpr int nPoints=100;		// how many frequency points; do not modify; call resizeVectors()
constexpr int startFreq=137500;	// start frequency in kHz
constexpr int freqStep=25000;		// frequency step in kHz
constexpr int nValues=50;

void* vna_dev=NULL;


typedef array<complex<double>,2> complex2;
typedef array<complex<double>,3> cal_t;



struct xavna_raw2_values {
	complex<double> ref,rawRefl,rawThru,refl,thru;
};
extern "C" int xavna_read_values_raw2(void* dev, xavna_raw2_values* out_values, int n_samples);

double freqAt(int i) {
	return double(startFreq+freqStep*i)/1000.;
}

inline string ssprintf(int maxLen, const char* fmt, ...) {
	string tmp(maxLen, '\0');
	va_list args;
    va_start(args, fmt);
    vsnprintf((char*)tmp.data(), maxLen, fmt, args);
    va_end(args);
    return tmp;
}
void setSwitch(int i) {
	digitalWrite(rfSwitchPins[0], (i>>2)&1);
	digitalWrite(rfSwitchPins[1], (i>>1)&1);
	digitalWrite(rfSwitchPins[2], (i)&1);
}
void setSwitchOpen() {
	setSwitch(0b001);
}
void setSwitchShort() {
	setSwitch(0b000);
}
void setSwitchLoad() {
	setSwitch(0b101);
}
void setSwitchThru() {
	setSwitch(0b010);
}

enum errorType {
	ERR_NO_DEV=1,		// device not detected
	ERR_COMMS=2,		// communication error/protocol error
	ERR_CONTROL=4,		// device reported error during control
	ERR_DATA=8,			// bad data received
	ERR_SPEC=16,		// device characteristics not within spec,
	ERR_BUG=32			// bug in the tester
};
void fail(errorType code, string msg) {
	fprintf(stderr, "FAIL: %s\n", msg.c_str());
	ui_show_details("FAIL", msg, 0xff0000);
	exit((int)code);
}
int errorCodes = 0;
string errorMsgs;
void appendFail(errorType code, string msg) {
	errorCodes |= (int)code;
	errorMsgs.append(msg);
	errorMsgs.append("\n");
	fprintf(stderr, "FAIL: %s\n", msg.c_str());
}
void showResult() {
	if(errorCodes == 0) {
		ui_show_banner("PASS", 0x00ff00, 0x0000ff);
	} else {
		ui_show_details("FAIL", errorMsgs, 0xff0000);
	}
}

#define BUG() fail(ERR_BUG, "line " + to_string(__LINE__) + ": " + string(strerror(errno)))

constexpr int queueSize=nPoints*3;
volatile xavna_raw2_values circularQueue[queueSize];
volatile int queueWriteIndex=0;
int queueWaitFD=-1;
volatile bool ignoreReadErrors=false;
void* refreshThread(void* v) {
	int x=0;
	while(true) {
		for(int i=0;i<nPoints;i++) {
			int freq = startFreq+freqStep*i;
			int freq2;
			xavna_raw2_values values;
			if((freq2=xavna_set_frequency(vna_dev,freq)) != freq) {
				fail(ERR_CONTROL, "device could not set frequency to " + to_string(freq)
					+ ", device reported " + to_string(freq2) + "; " + string(strerror(errno)));
			}
			if(xavna_read_values_raw2(vna_dev,&values,nValues)<=0) {
				if(ignoreReadErrors) return NULL;
				fail(ERR_COMMS, "xavna_read_values returned no values: " + string(strerror(errno)));
			}
			//circularQueue[queueWriteIndex] = values;
			memcpy((void*)(circularQueue+queueWriteIndex), &values, sizeof(values));
			__sync_synchronize();
			queueWriteIndex = (queueWriteIndex+1)<queueSize?(queueWriteIndex+1):0;
			
			uint64_t val=1;
			write(queueWaitFD, &val, sizeof(val));
		}
	}
}

vector<xavna_raw2_values> doScan(int discardPoints=10) {
	int initialIndex = queueWriteIndex;
	__sync_synchronize();
	uint64_t val=0;
	while(true) {
		if(read(queueWaitFD, &val, sizeof(val)) != sizeof(val)) {
			BUG();
		}
		int length = (queueWriteIndex-initialIndex+queueSize) % queueSize;
		if(length>=(nPoints+discardPoints)) break;
	}
	__sync_synchronize();
	vector<xavna_raw2_values> res(nPoints, {0,0,0,0,0});
	
	for(int i=0;i<nPoints;i++) {
		int index = (initialIndex + discardPoints + i) % queueSize;
		memcpy(&res[index%nPoints], (void*)&circularQueue[index], sizeof(xavna_raw2_values));
		//res[index%nPoints] = circularQueue[index];
	}
	return res;
}
/*
vector<xavna_raw2_values> doScan() {
	vector<xavna_raw2_values> res;
	res.resize(nPoints);
	xavna_raw2_values values;
	
	
	for(int i=0;i<nPoints;i++) {
		int freq = startFreq+freqStep*i;
		int freq2;
		if((freq2=xavna_set_frequency(vna_dev,freq)) != freq) {
			fail(ERR_CONTROL, "device could not set frequency to " + to_string(freq)
				+ ", device reported " + to_string(freq2) + "; " + string(strerror(errno)));
		}
		if(xavna_read_values_raw2(vna_dev,&values,nValues)<=0) {
			fail(ERR_COMMS, "xavna_read_values returned no values: " + string(strerror(errno)));
		}
		res[i] = values;
	}
	return res;
}*/

double dB(double pwr) {
	return log10(pwr)*10;
}
// returns directivity in dB
double directivity(complex<double> S, complex<double> O, complex<double> L) {
	auto dir1 = dB(norm(S)/norm(L));
	auto dir2 = dB(norm(O)/norm(L));
	if(dir1<dir2) return dir1;
	return dir2;
}
// returns sensitivity of reflection coefficient vs raw values in dB
double sensitivity(cal_t coeffs) {
	double worst = 0;
	for(int i=0;i<360;i++) {
		double tmp = abs(SOL_compute_sensitivity(coeffs, polar(1., double(i)/180*M_PI)));
		if(tmp>worst) worst = tmp;
	}
	return dB(worst)*2;
}



int main(int argc, char** argv) {
	if(argc<2) {
		fprintf(stderr,"usage: %s /PATH/TO/TTY\n",argv[0]);
		return 1;
	}
	wiringPiSetup();
	pinMode(rfSwitchPins[0], OUTPUT);
	pinMode(rfSwitchPins[1], OUTPUT);
	pinMode(rfSwitchPins[2], OUTPUT);
	
	ui_init();
	
	vna_dev = xavna_open(argv[1]);
	if(vna_dev == NULL) {
		ui_show_banner("No device");
		while(true) {
			vna_dev = xavna_open(argv[1]);
			if(vna_dev != NULL) break;
			usleep(300000);
		}
	}
	
	queueWaitFD = eventfd(0, 0);
	pthread_t refreshThread_;
	if(pthread_create(&refreshThread_, NULL, &refreshThread, NULL) != 0)
		BUG();
	
	vector<xavna_raw2_values> values_short,values_open,values_load,values_load2,values_thru;
	
	ui_show_banner("WARMUP", 0xbb5500);
	setSwitchShort();
	doScan();
	doScan();
	ui_show_banner("0%", 0xbb5500);
	setSwitchShort();
	values_short = doScan();
	ui_show_banner("20%", 0xbb5500);
	setSwitchOpen();
	values_open = doScan();
	ui_show_banner("40%", 0xbb5500);
	setSwitchThru();
	values_thru = doScan();
	ui_show_banner("60%", 0xbb5500);
	setSwitchLoad();
	values_load = doScan();
	ui_show_banner("80%", 0xbb5500);
	setSwitchLoad();
	values_load2 = doScan();
	
	vector<cal_t> cal_coeffs(nPoints);
	
	double worst_directivity = 100;
	int worst_directivity_freq = 0;
	double worst_sensitivity = 0;
	int worst_sensitivity_freq = 0;
	vector<double> s11floors(nPoints);
	vector<int> worst_s11floors(nPoints);
	for(int i=0;i<nPoints;i++) {
		int freq = startFreq + i*freqStep;
		auto S = values_short[i].refl, O = values_open[i].refl, L = values_load[i].refl;
		cal_coeffs[i] = SOL_compute_coefficients(S,O,L);
		
		double dir = directivity(S, O, L);
		double sens = sensitivity(SOL_compute_coefficients(
			values_short[i].rawRefl,values_open[i].rawRefl,values_load[i].rawRefl));
		double s11floor = dB(norm(SOL_compute_reflection(cal_coeffs[i], values_load2[i].refl)));
		
		if(dir < worst_directivity) {
			worst_directivity = dir;
			worst_directivity_freq = i;
		}
		if(sens > worst_sensitivity) {
			worst_sensitivity = sens;
			worst_sensitivity_freq = i;
		}
		printf("%.2fMHz: directivity = %.1fdB, ", double(freq)/1000., dir);
		printf("sensitivity = %.1fdB, ", sens);
		printf("S11 = %.1fdB\n", s11floor);
		s11floors[i] = s11floor;
		worst_s11floors[i] = i;
	}
	sort(worst_s11floors.begin(), worst_s11floors.end(), [&](int a, int b) {return s11floors[a] > s11floors[b];});
	
	printf("worst directivity: %.1fdB @ %.2fMHz\n", worst_directivity, freqAt(worst_directivity_freq));
	printf("worst sensitivity: %.1fdB @ %.2fMHz\n", worst_sensitivity, freqAt(worst_sensitivity_freq));
	int s11floor0 = worst_s11floors[0];
	int s11floor2 = worst_s11floors[2];
	printf("highest S11: %.1fdB @ %.2fMHz\n", s11floors[s11floor0], freqAt(s11floor0));
	printf("3rd highest S11: %.1fdB @ %.2fMHz\n", s11floors[s11floor2], freqAt(s11floor2));
	
	if(s11floors[s11floor0] > -40) {
		string msg = ssprintf(255, "S11 floor too high: \n    %.1fdB @ %.2fMHz", s11floors[s11floor0], freqAt(s11floor0));
		appendFail(ERR_SPEC, msg);
	}
	if(s11floors[s11floor2] > -50) {
		string msg = ssprintf(255, "3rd highest S11 too high: \n    %.1fdB @ %.2fMHz", s11floors[s11floor2], freqAt(s11floor2));
		appendFail(ERR_SPEC, msg);
	}
	if(worst_sensitivity > 30) {
		string msg = ssprintf(255, "S11 sensitivity too high: \n    %.1fdB @ %.2fMHz", worst_sensitivity, freqAt(worst_sensitivity_freq));
		appendFail(ERR_SPEC, msg);
	}
	showResult();
	// wait for device unplug
	printf("waiting for device unplug\n");
	ignoreReadErrors = true;
	pthread_join(refreshThread_, NULL);
	return errorCodes;
}

