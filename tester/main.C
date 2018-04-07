#include <wiringPi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>

#include <array>
#include <vector>
#include <complex>
#include <string>
#include <functional>
#include <iostream>
#include <fstream>

#include <xavna/xavna.h>
#include <xavna/calibration.H>

#include "ui.H"
#include "platform.H"
#include "svfplayer/libsvfplayer.H"

using namespace std;

int rfSwitchPins[3] = {7,8,9};
constexpr int nPoints=50;		// how many frequency points
constexpr int startFreq=137500;	// start frequency in kHz
constexpr int freqStep=50000;		// frequency step in kHz
constexpr int nValues=50;
bool detailed = true;
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
string errorMsgs,infoMsgs;
void appendFail(errorType code, string msg) {
	errorCodes |= (int)code;
	errorMsgs.append(msg);
	errorMsgs.append("\n");
	fprintf(stderr, "FAIL: %s\n", msg.c_str());
}
void appendInfo(string msg) {
	infoMsgs.append(msg);
	infoMsgs.append("\n");
	fprintf(stderr, "%s\n", msg.c_str());
}
void showResult() {
	if(errorCodes == 0) {
		if(detailed)
			ui_show_details("PASS", infoMsgs, 0x00ff00, 0x0000ff);
		else
			ui_show_banner("PASS", 0x00ff00, 0x0000ff);
	} else {
		ui_show_details("FAIL", errorMsgs, 0xff0000);
	}
}

#define BUG() fail(ERR_BUG, "line " + to_string(__LINE__) + ": " + string(strerror(errno)))

// hack to make it easy to start a thread with a std::function
void* callStdFunction(void* param) {
	function<void()>* func = (function<void()>*) param;
	(*func)();
}

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
			if(xavna_set_params(vna_dev,freq, 20, -1) < 0) {
				fail(ERR_CONTROL, "device could not set parameters; frequency = " + to_string(freq)
					+ "; " + string(strerror(errno)));
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

string svfToWaveform(string svf) {
	svfParser parser;
	svfPlayer player;
	parser.reset();
	player.reset();
	
	std::stringstream ss(svf);
    std::string item;
    int cmds=0;
    while (std::getline(ss, item, '\n')) {
        parser.processLine(item.c_str(),item.length());
		svfCommand cmd;
		while(parser.nextCommand(cmd)) {
			player.processCommand(cmd);
			cmds++;
		}
    }
	return player.outBuffer;
}

int playSvf(string svf) {
	string waveform = svfToWaveform(svf);
	return platform_writeJtag((uint8_t*)waveform.data(),waveform.length());
}

bool detectJtag() {
	int res = playSvf(
		"ENDIR IDLE; ENDDR IDLE; STATE RESET; STATE IDLE;\
		SIR 6 TDI (09) SMASK (3f);\
		SDR 32 TDI (00000000) SMASK (ffffffff) TDO (f4001093) MASK (0fffffff);");
	if(res == 0) {
		fprintf(stderr, "FPGA detected on JTAG\n");
	}
	return res==0;
}
void doProgram() {
	ifstream t("a.svf");
	string str((istreambuf_iterator<char>(t)),
					 istreambuf_iterator<char>());
	string waveform = svfToWaveform(str);
	fprintf(stderr, "loaded svf; %d bytes; total jtag cycles: %d\n", str.length(), waveform.length());
	
	if(waveform.length() == 0) {
		ui_show_details("ERROR", "No a.svf file found", 0xff0000);
		return;
	}
	
	int blockSize=500000;
	for(int i=0;i<waveform.length();i+=blockSize) {
		int len=waveform.length()-i;
		if(len>blockSize) len=blockSize;
		int ret = platform_writeJtag((uint8_t*)waveform.data()+i,len);
		if(ret != 0) {
			string msg = ssprintf(64, "jtag error: %d", ret);
			ui_show_details("ERROR", msg, 0xff0000);
			return;
		}
		int percent = (i+len)*100/waveform.length();
		ui_show_banner(ssprintf(64, "%d%%", percent), 0xbb5500);
	}
	ui_show_details("Done", "Please remove JTAG cable", 0x00ff00, 0x0000ff);
}
void tryProgram() {
	for(int i=0; i<5; i++) {
		if(!detectJtag()) return;
		usleep(500000);
	}
	ui_show_banner("Programming", 0xbb5500);
	doProgram();
	
	
	// wait for jtag disconnect; proceed when device has been undetectable 5 times
	int cnt=0;
	while(true) {
		if(detectJtag()) cnt=0;
		else cnt++;
		if(cnt>5) break;
		usleep(300000);
	}
}

int main(int argc, char** argv) {
	if(argc<2) {
		fprintf(stderr,"usage: %s /PATH/TO/TTY\n",argv[0]);
		return 1;
	}
	wiringPiSetup();
	platform_init();
	pinMode(rfSwitchPins[0], OUTPUT);
	pinMode(rfSwitchPins[1], OUTPUT);
	pinMode(rfSwitchPins[2], OUTPUT);
	
	ui_init();
	
	vna_dev = xavna_open(argv[1]);
	if(vna_dev == NULL) {
		while(true) {
			usleep(500000);
			
			tryProgram();
			
			vna_dev = xavna_open(argv[1]);
			if(vna_dev != NULL) break;
			ui_show_banner("No device");
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
	vector<double> s21dr(nPoints);
	vector<int> worst_s21dr(nPoints);
	for(int i=0;i<nPoints;i++) {
		int freq = startFreq + i*freqStep;
		auto S = values_short[i].refl, O = values_open[i].refl, L = values_load[i].refl;
		cal_coeffs[i] = SOL_compute_coefficients(S,O,L);
		
		double dir = directivity(S, O, L);
		double sens = sensitivity(SOL_compute_coefficients(
			values_short[i].rawRefl,values_open[i].rawRefl,values_load[i].rawRefl));
		double s11floor = dB(norm(SOL_compute_reflection(cal_coeffs[i], values_load2[i].refl)));
		s21dr[i] = dB(norm(values_thru[i].thru)) - dB(norm(values_load[i].thru));
		worst_s21dr[i] = i;
		
		if(dir < worst_directivity && freq<1000000) {
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
	sort(worst_s21dr.begin(), worst_s21dr.end(), [&](int a, int b) {return s21dr[a] > s21dr[b];});
	
	int s11floor0 = worst_s11floors[0];
	int s11floor2 = worst_s11floors[2];
	int s21dr2 = worst_s21dr[2];
	
	appendInfo(ssprintf(255, "worst directivity: \n    %.1fdB @ %.2fMHz", worst_directivity, freqAt(worst_directivity_freq)));
	appendInfo(ssprintf(255, "worst error sensitivity: \n    %.1fdB @ %.2fMHz", worst_sensitivity, freqAt(worst_sensitivity_freq)));
	appendInfo(ssprintf(255, "3rd worst dynamic range: \n    %.1fdB @ %.2fMHz", s21dr[s21dr2], freqAt(s21dr2)));
	appendInfo(ssprintf(255, "highest S11: \n    %.1fdB @ %.2fMHz", s11floors[s11floor0], freqAt(s11floor0)));
	appendInfo(ssprintf(255, "3rd highest S11: \n    %.1fdB @ %.2fMHz", s11floors[s11floor2], freqAt(s11floor2)));
	
	if(s11floors[s11floor0] > -40 || isnan(s11floors[s11floor0])) {
		string msg = ssprintf(255, "S11 floor too high: \n    %.1fdB @ %.2fMHz", s11floors[s11floor0], freqAt(s11floor0));
		appendFail(ERR_SPEC, msg);
	}
	if(s11floors[s11floor2] > -50 || isnan(s11floors[s11floor2])) {
		string msg = ssprintf(255, "3rd highest S11 too high: \n    %.1fdB @ %.2fMHz", s11floors[s11floor2], freqAt(s11floor2));
		appendFail(ERR_SPEC, msg);
	}
	if(worst_sensitivity > 30 || isnan(worst_sensitivity)) {
		string msg = ssprintf(255, "S11 errorsens too high: \n    %.1fdB @ %.2fMHz", worst_sensitivity, freqAt(worst_sensitivity_freq));
		appendFail(ERR_SPEC, msg);
	}
	if(s21dr[s21dr2] < 30 || isnan(s21dr[s21dr2])) {
		string msg = ssprintf(255, "S21 dynamic range too low: \n    %.1fdB @ %.2fMHz", s21dr[s21dr2], freqAt(s21dr2));
		appendFail(ERR_SPEC, msg);
	}
	showResult();
	// wait for device unplug
	printf("waiting for device unplug\n");
	ignoreReadErrors = true;
	pthread_join(refreshThread_, NULL);
	return errorCodes;
}

