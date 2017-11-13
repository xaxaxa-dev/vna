
#include "polar_view.H"
#include "graph_view.H"
#include <gtkmm/application.h>
#include <gtkmm/window.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include "common_types.h"
#include <string>
#include <stdarg.h>
#include <termios.h>
#include <fftw3.h>

//#include "adf4350_board.H"

using namespace std;
//using namespace adf4350Board;

#define TOKEN_TO_STRING(TOK) # TOK
#define GETWIDGET(x) builder->get_widget(TOKEN_TO_STRING(x), x)

#define FFTWFUNC(x) fftw_ ## x

// settings
int nPoints=50;
int startFreq=17000;
int freqStep=200;
double freqMultiplier=0.1; //19.2/4;
bool timeDomain = false;
bool showCursor = true;

int nWait=50;		//number of data points to skip after changing frequency
int nValues=150;	//number of data points to integrate over
int nWaitExtended=150;
int nValuesExtended=250;


// globals

// to be declared in the generated vna.glade.c file
extern unsigned char vna_glade[];
extern unsigned int vna_glade_len;

int ttyFD=-1;
Glib::RefPtr<Gtk::Builder> builder;
xaxaxa::PolarView* polarView=NULL;
xaxaxa::GraphView* graphView=NULL;
xaxaxa::GraphView* timeGraphView=NULL;
pthread_t refreshThread;
bool refreshThreadShouldExit=false;

double Z0=50.;
bool use_cal=false;

vector<complex<double> > cal_oc,cal_sc,cal_t;	// measured raw values for the 3 calib references
vector<complex<double> > cal_X,cal_Y,cal_Z;		// the 3 calibration terms
vector<complex<double> > cal_thru;				// the raw value for the thru reference

struct complex5 {
	complex<double> val[5];
};

// forward declarations
void updateFreqButton();
void resizeVectors();

string ssprintf(int maxLen, const char* fmt, ...) {
	string tmp(maxLen, '\0');
	va_list args;
    va_start(args, fmt);
    vsnprintf((char*)tmp.data(), maxLen, fmt, args);
    va_end(args);
    return tmp;
}
double dB(double power) {
	return log10(power)*10;
}
double gauss(double x, double m, double s) {
    static const double inv_sqrt_2pi = 0.3989422804014327;
    double a = (x - m) / s;
    return inv_sqrt_2pi / s * std::exp(-0.5d * a * a);
}

int timePoints() {
	return (nPoints*3-1);
}

// returns MHz
double freqAt(int i) {
	return (startFreq+i*freqStep)*freqMultiplier;
}
// returns ns
double timeAt(int i) {
	double fs=double(freqStep)*freqMultiplier; // MHz
	double totalTime = 1000./fs/2; // ns
	return double(i)*totalTime/double(timePoints())/2;
}


//writes the commands in the buffer to stdout
void doWriteSPI(string buf) {
	//fill in the register address in every byte and set the data enable bit
	for(int i=0;i<(int)buf.length();i++)
		buf[i] |= 2 << 4 | 1<<3;
	assert(write(ttyFD,buf.data(),buf.length())==(int)buf.length());
}

u64 sx(u64 data, int bits) {
	u64 mask=~u64(u64(1LL<<bits) - 1);
	return data|((data>>(bits-1))?mask:0);
}

void setFrequency(int N) {
	u8 buf[8] = {
		1, u8(N>>8),
		2, u8(N),
		0, 0,
		3, 1
	};
	assert(write(ttyFD,buf,sizeof(buf))==(int)sizeof(buf));
}

double avgRe=0,avgIm=0;
complex<double> processValue(u64 data1,u64 data2) {
	data1=sx(data1,35);
	data2=sx(data2,35);
	
	return {double((ll)data2), double((ll)data1)};
}

complex<double> readValue(int cnt) {
	complex<double> result=0;
	int bufsize=1024;
	u8 buf[bufsize];
	int br;
	u64 data1,data2;
	u64* curData=&data1;
	int n=0,j=0;
	
	u8 msb=1<<7;
	u8 mask=msb-1;
	while((br=read(ttyFD,buf,sizeof(buf)))>0) {
		for(int i=0;i<br;i++) {
			if((buf[i]&msb)==0) {
				result+=processValue(data1,data2);
				if(++n >= cnt) return result;
				data1=0;
				data2=0;
				curData=&data1;
				j=0;
			}
			(*curData)|=u64(buf[i]&mask) << j;
			j+=7;
			if(j>=35) {
				j=0;
				curData=&data2;
			}
		}
	}
	return 0;
}

// returns [adc0, adc1, adc2, adc1/adc0, adc2/adc0]
complex5 readValue3(int cnt) {
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
						return result;
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
	return result;
}


// increment this variable to request the thread take an extended measurement (for when more accuracy is required)
volatile int requestedMeasurements=0;
// function to be called from the main thread when a requested measurement is complete
function<void(vector<complex5>)>* volatile measurementCallback;

void* thread1(void* v) {
	int requestedMeasurementsPrev=requestedMeasurements;
	
	complex<double>* reflArray = (complex<double>*)FFTWFUNC(malloc)(nPoints*3*sizeof(complex<double>));
	complex<double>* thruArray = (complex<double>*)FFTWFUNC(malloc)(nPoints*3*sizeof(complex<double>));
	double* reflTD = (double*)FFTWFUNC(malloc)(timePoints()*2*sizeof(double));
	double* thruTD = (double*)FFTWFUNC(malloc)(timePoints()*2*sizeof(double));
	
	FFTWFUNC(plan) p1, p2;
	p1 = fftw_plan_dft_c2r_1d(timePoints()*2, (fftw_complex*)reflArray, reflTD, 0);
	p2 = fftw_plan_dft_c2r_1d(timePoints()*2, (fftw_complex*)thruArray, thruTD, 0);
	
	while(true) {
		memset(reflArray, 0, nPoints*3*sizeof(complex<double>));
		memset(thruArray, 0, nPoints*3*sizeof(complex<double>));
		
		int N=startFreq;
		for(int i=0;i<nPoints;i++) {
			//printf("%d\n",N);
			setFrequency(N);
			readValue3(nWait);
			
			
			complex5 values=readValue3(nValues);
			complex<double> reflValue=values.val[4];
			complex<double> thruValue=values.val[3];
			printf("%7.2f %20lf %10lf %20lf %10lf\n",N*freqMultiplier,
				abs(values.val[2]),arg(values.val[2]),abs(values.val[0]),arg(values.val[0]));
			auto refl = reflValue;
			auto thru = thruValue;
			if(use_cal) {
				//auto refl=(cal_X[i]*cal_Y[i]-value*cal_Z[i])/(value-cal_X[i]);
				refl=(cal_X[i]*cal_Y[i]-reflValue)/(reflValue*cal_Z[i]-cal_X[i]);
				thru=thruValue/cal_thru[i];
			}
			
			reflArray[i] = refl * gauss(double(i)/nPoints, 0, 0.7);
			thruArray[i] = thru * gauss(double(i)/nPoints, 0, 0.7);
			polarView->points[i]=refl;
			graphView->lines[0][i] = arg(refl);
			graphView->lines[1][i] = arg(thru);
			graphView->lines[2][i] = dB(norm(refl));
			graphView->lines[3][i] = dB(norm(thru));
			
			N+=freqStep;
			if(requestedMeasurements>requestedMeasurementsPrev) break;
			if(refreshThreadShouldExit) return NULL;
		}
		// compute time domain values
		FFTWFUNC(execute)(p1);
		FFTWFUNC(execute)(p2);
		int tPoints = timePoints();
		for(int i=0;i<tPoints;i++) {
			auto refl = reflTD[i]/nPoints;
			auto thru = thruTD[i]/nPoints;
			timeGraphView->lines[0][i] = dB(refl*refl);
			timeGraphView->lines[1][i] = dB(thru*thru);
			printf("%d %.10lf\n",i,reflTD[i]);
		}
		
		Glib::signal_idle().connect([]() {
			polarView->commitTrace();
			return false;
		});
		
		// take extended measurement
		if(requestedMeasurements>requestedMeasurementsPrev) {
			requestedMeasurementsPrev = requestedMeasurements;
			__sync_synchronize();
			
			printf("taking extended measurement...\n");
			vector<complex5> results;
			N=startFreq;
			for(int i=0;i<nPoints;i++) {
				setFrequency(N);
				readValue3(nWaitExtended);
				complex5 values=readValue3(nValuesExtended);
				results.push_back(values);
				
				printf("%7.2f %20lf %10lf %20lf %10lf\n",N*freqMultiplier,
					abs(values.val[2]),arg(values.val[2]),abs(values.val[0]),arg(values.val[0]));
				
				polarView->points[i]=values.val[4];
				graphView->lines[1][i] = arg(values.val[3]);
				graphView->lines[3][i] = dB(norm(values.val[3]));
				
				N+=freqStep;
			}
			printf("done.\n");
			Glib::signal_idle().connect([results]() {
				function<void(vector<complex5>)> cb = *measurementCallback;
				delete measurementCallback;
				measurementCallback = NULL;
				cb(results);
				return false;
			});
		}
	}
}

void takeMeasurement(function<void(vector<complex5>)> cb) {
	function<void(vector<complex5>)>* cbNew = new function<void(vector<complex5>)>();
	*cbNew = cb;
	__sync_synchronize();
	measurementCallback = cbNew;
	__sync_synchronize();
	__sync_add_and_fetch(&requestedMeasurements, 1);
}

void alert(string msg) {
	Gtk::Window* window1;
	GETWIDGET(window1);
	Gtk::MessageDialog dialog(*window1, msg,
		false /* use_markup */, Gtk::MESSAGE_WARNING,
		Gtk::BUTTONS_OK);
	dialog.run();
}

string saveCalibration() {
	string tmp;
	tmp += '\x02';		// file format version
	tmp.append((char*)&nPoints, sizeof(nPoints));
	tmp.append((char*)&startFreq, sizeof(startFreq));
	tmp.append((char*)&freqStep, sizeof(freqStep));
	tmp.append((char*)cal_X.data(), sizeof(cal_X[0]) * nPoints);
	tmp.append((char*)cal_Y.data(), sizeof(cal_Y[0]) * nPoints);
	tmp.append((char*)cal_Z.data(), sizeof(cal_Z[0]) * nPoints);
	tmp.append((char*)cal_thru.data(), sizeof(cal_thru[0]) * nPoints);
	return tmp;
}
bool loadCalibration(char* data, int size) {
	int calSize=sizeof(cal_X[0]) * nPoints;
	if(size<=0) {
		alert("invalid/corrupt calibration file; file is empty");
		return false;
	}
	if(data[0] != 2) {
		alert("incorrect calibration file version; should be 1, is " + to_string((int)data[0]));
		return false;
	}
	if(size < 13+calSize*4) {
		alert("file corrupt; length too short");
		return false;
	}
	int nPoints1, startFreq1, freqStep1;
	nPoints1 = *(int*)(data+1);
	startFreq1 = *(int*)(data+5);
	freqStep1 = *(int*)(data+9);
	if(nPoints1 != nPoints || startFreq1 != startFreq || freqStep1 != freqStep) {
		alert(ssprintf(128, "calibration file has different parameters: %d points, start %d, step %d", nPoints1, startFreq1, freqStep1));
		return false;
	}
	
	
	memcpy(cal_X.data(), data+13, calSize);
	memcpy(cal_Y.data(), data+13+calSize, calSize);
	memcpy(cal_Z.data(), data+13+calSize*2, calSize);
	memcpy(cal_thru.data(), data+13+calSize*3, calSize);
	use_cal = true;
	return true;
}


void addButtonHandlers() {
	// controls
	Gtk::Window *window1, *window3;
	Gtk::Button *b_oc, *b_sc, *b_t, *b_thru, *b_apply, *b_clear, *b_load, *b_save, *b_freq;
	Gtk::ToggleButton *c_persistence, *c_freq, *c_ttf;
	
	// get controls
	GETWIDGET(window1); GETWIDGET(window3);
	GETWIDGET(b_oc); GETWIDGET(b_sc); GETWIDGET(b_t); GETWIDGET(b_thru);
	GETWIDGET(b_apply); GETWIDGET(b_clear); 
	GETWIDGET(b_load); GETWIDGET(b_save);
	GETWIDGET(b_freq); GETWIDGET(c_persistence);
	GETWIDGET(c_freq); GETWIDGET(c_ttf);
	b_oc->signal_clicked().connect([window1]() {
		for(int i=0;i<nPoints;i++) polarView->points[i] = NAN;
		window1->set_sensitive(false);
		takeMeasurement([window1](vector<complex5> values) {
			for(int i=0;i<nPoints;i++)
				cal_oc[i] = values[i].val[4];
			window1->set_sensitive(true);
		});
	});
	b_sc->signal_clicked().connect([window1]() {
		for(int i=0;i<nPoints;i++) polarView->points[i] = NAN;
		window1->set_sensitive(false);
		takeMeasurement([window1](vector<complex5> values) {
			for(int i=0;i<nPoints;i++)
				cal_sc[i] = values[i].val[4];
			window1->set_sensitive(true);
		});
	});
	b_t->signal_clicked().connect([window1]() {
		for(int i=0;i<nPoints;i++) polarView->points[i] = NAN;
		window1->set_sensitive(false);
		takeMeasurement([window1](vector<complex5> values) {
			for(int i=0;i<nPoints;i++)
				cal_t[i] = values[i].val[4];
			window1->set_sensitive(true);
		});
	});
	b_thru->signal_clicked().connect([window1]() {
		for(int i=0;i<nPoints;i++) graphView->lines[3][i] = NAN;
		window1->set_sensitive(false);
		takeMeasurement([window1](vector<complex5> values) {
			for(int i=0;i<nPoints;i++)
				cal_thru[i] = values[i].val[3];
			window1->set_sensitive(true);
		});
	});
	b_apply->signal_clicked().connect([]() {
		for(int i=0;i<nPoints;i++) {
			complex<double> a=cal_t[i], b=cal_oc[i], c=cal_sc[i];
			/*
			cal_Z[i]=(c-b)/(b+c-2.d*a);
			cal_X[i]=cal_Z[i]*(a-c) + c;
			cal_Y[i]=a*cal_Z[i]/cal_X[i];
			*/
			cal_Z[i]=(2.d*a-b-c)/(b-c);
			cal_X[i]=a-c*(1.d-cal_Z[i]);
			cal_Y[i]=a/cal_X[i];
			printf("%.f: X=(%lf, %lf) Y=(%lf, %lf), Z=(%lf, %lf)\n", freqAt(i), 
					cal_X[i].real(), cal_X[i].imag(),
					cal_Y[i].real(), cal_Y[i].imag(), 
					cal_Z[i].real(), cal_Z[i].imag());
		}
		use_cal=true;
	});
	b_clear->signal_clicked().connect([]() {
		use_cal=false;
	});
	
	b_save->signal_clicked().connect([window1]() {
		Gtk::FileChooserDialog d(*window1, "Save calibration file...", FILE_CHOOSER_ACTION_SAVE);
		Gtk::Button* b = d.add_button(Stock::SAVE, RESPONSE_OK);
		if(d.run() == RESPONSE_OK)
		{
			string data=saveCalibration();
			string etag;
			d.get_file()->replace_contents(data, "", etag);
		}
	});
	b_load->signal_clicked().connect([window1]() {
		Gtk::FileChooserDialog d(*window1, "Open calibration file...", FILE_CHOOSER_ACTION_OPEN);
		Gtk::Button* b = d.add_button(Stock::OPEN, RESPONSE_OK);
		if(d.run() == RESPONSE_OK)
		{
			char* fileData = NULL;
			gsize sz = 0;
			d.get_file()->load_contents(fileData, sz);
			loadCalibration(fileData, sz);
			free(fileData);
		}
	});
	b_freq->signal_clicked().connect([window1]() {
		Gtk::Dialog* dialog_freq;
		Gtk::Entry *d_t_start, *d_t_step, *d_t_span;
		GETWIDGET(dialog_freq);
		GETWIDGET(d_t_start); GETWIDGET(d_t_step); GETWIDGET(d_t_span);
		
		d_t_start->set_text(ssprintf(20, "%.1f", double(startFreq)*freqMultiplier));
		d_t_step->set_text(ssprintf(20, "%.1f", double(freqStep)*freqMultiplier));
		d_t_span->set_text(ssprintf(20, "%d", nPoints));
		
		dialog_freq->set_transient_for(*window1);
		if(dialog_freq->run() == RESPONSE_OK) {
			refreshThreadShouldExit = true;
			pthread_join(refreshThread, NULL);
			refreshThreadShouldExit = false;
			
			Gtk::Entry *d_t_start, *d_t_step, *d_t_span;
			GETWIDGET(d_t_start); GETWIDGET(d_t_step); GETWIDGET(d_t_span);
			auto start = d_t_start->get_text();
			auto step = d_t_step->get_text();
			auto span = d_t_span->get_text();
			startFreq = atof(start.c_str())/freqMultiplier;
			freqStep = atof(step.c_str())/freqMultiplier;
			nPoints = atoi(span.c_str());
			
			resizeVectors();
			updateFreqButton();
			use_cal = false;
			
			if(pthread_create(&refreshThread, NULL, &thread1,NULL)<0) {
				perror("pthread_create");
				exit(1);
			}
		}
		dialog_freq->hide();
	});
	c_persistence->signal_toggled().connect([c_persistence]() {
		polarView->persistence = c_persistence->get_active();
		if(polarView->persistence)
			polarView->clearPersistence();
	});
	c_freq->signal_toggled().connect([c_freq]() {
		showCursor = c_freq->get_active();
		if(showCursor) {
			polarView->cursorColor = 0xffffff00;
		} else {
			polarView->cursorColor = 0x00000000;
		}
		polarView->queue_draw();
	});
	c_ttf->signal_toggled().connect([c_ttf, window3]() {
		if(c_ttf->get_active())
			window3->show();
		else window3->hide();
	});
}

string GetDirFromPath(const string path)
{
	int i = path.rfind("/");
	if (i < 0) return string();
	return path.substr(0, i + 1);
}
string GetProgramPath()
{
	char buf[256];
	int i = readlink("/proc/self/exe", buf, sizeof(buf));
	if (i < 0) {
		perror("readlink");
		return "";
	}
	return string(buf, i);
}

// freq is in Hz, Z is in ohms
double capacitance_inductance(double freq, double Z) {
	if(Z>0) return Z/(2*M_PI*freq);
	return 1./(2*Z*M_PI*freq);
}
// freq is in Hz, Y is in mhos
double capacitance_inductance_Y(double freq, double Y) {
	if(Y<0) return -1./(2*Y*M_PI*freq);
	return -Y/(2*M_PI*freq);
}
double si_scale(double val) {
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
const char* si_unit(double val) {
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

void updateLabels() {
	Gtk::Scale *s_freq;
	Gtk::Label *l_freq,*l_refl,*l_refl_phase,*l_through,*l_through_phase;
	Gtk::Label *l_impedance, *l_admittance, *l_s_admittance, *l_p_impedance, *l_series, *l_parallel;
	Gtk::ToggleButton *c_freq;
	GETWIDGET(s_freq); GETWIDGET(c_freq); GETWIDGET(l_refl); GETWIDGET(l_refl_phase);
	GETWIDGET(l_through); GETWIDGET(l_through_phase);
	GETWIDGET(l_impedance); GETWIDGET(l_admittance); GETWIDGET(l_s_admittance);
	GETWIDGET(l_p_impedance); GETWIDGET(l_series); GETWIDGET(l_parallel);
	
	// frequency label
	int freqIndex=(int)s_freq->get_value();
	double freq=freqAt(freqIndex);
	c_freq->set_label(ssprintf(20, "%.1f MHz", freq));
	
	// impedance display panel (left side)
	if(!use_cal) {
		l_refl->set_text("");
		l_refl_phase->set_text("");
		l_through->set_text("");
		l_through_phase->set_text("");
		l_impedance->set_text("");
		l_admittance->set_text("");
		l_s_admittance->set_text("");
		l_p_impedance->set_text("");
		l_series->set_text("");
		l_parallel->set_text("");
		return;
	}
	complex<double> reflCoeff = polarView->points[freqIndex];
	l_refl->set_text(ssprintf(20, "%.1f dB", dB(norm(reflCoeff))));
	l_refl_phase->set_text(ssprintf(20, "%.1f °", arg(reflCoeff)*180/M_PI));
	
	l_through->set_text(ssprintf(20, "%.1f dB", graphView->lines[3][freqIndex]));
	l_through_phase->set_text(ssprintf(20, "%.1f °", graphView->lines[1][freqIndex]*180/M_PI));
	
	complex<double> Z = -Z0*(reflCoeff+1.)/(reflCoeff-1.);
	complex<double> Y = -(reflCoeff-1.)/(Z0*(reflCoeff+1.));
	
	l_impedance->set_text(ssprintf(127, "  %.2f\n%s j%.2f", Z.real(), Z.imag()>=0 ? "+" : "-", fabs(Z.imag())));
	l_admittance->set_text(ssprintf(127, "  %.4f\n%s j%.4f", Y.real(), Y.imag()>=0 ? "+" : "-", fabs(Y.imag())));
	l_s_admittance->set_text(ssprintf(127, "  %.4f\n%s j%.4f", 1./Z.real(), Z.imag()>=0 ? "+" : "-", fabs(1./Z.imag())));
	l_p_impedance->set_text(ssprintf(127, "  %.2f\n|| j%.2f", 1./Y.real(), 1./Y.imag()));
	
	double value = capacitance_inductance(freq*1e6, Z.imag());
	l_series->set_text(ssprintf(127, "%.2f Ω\n%.2f %s%s", Z.real(), fabs(si_scale(value)), si_unit(value), value>0?"H":"F"));
	
	value = capacitance_inductance_Y(freq*1e6, Y.imag());
	l_parallel->set_text(ssprintf(127, "%.2f Ω\n%.2f %s%s", 1./Y.real(), fabs(si_scale(value)), si_unit(value), value>0?"H":"F"));
}
void updateLabels_ttf() {
	Gtk::Scale *s_time;
	Gtk::ToggleButton *c_time;
	Gtk::Label *l_refl1,*l_through1;
	GETWIDGET(s_time); GETWIDGET(c_time);
	GETWIDGET(l_refl1); GETWIDGET(l_through1);
	
	// time label
	int timeIndex=(int)s_time->get_value();
	double t=timeAt(timeIndex);
	c_time->set_label(ssprintf(20, "%.2f ns", t));
	
	// reflection and through labels
	double refl = timeGraphView->lines[0][timeIndex];
	double through = timeGraphView->lines[1][timeIndex];
	l_refl1->set_text(ssprintf(20, "%.1f dB", refl));
	l_through1->set_text(ssprintf(20, "%.1f dB", through));
	
}


void updateFreqButton() {
	Gtk::Button *b_freq;
	GETWIDGET(b_freq);
	b_freq->set_label(ssprintf(31, "%.1f MHz -\n%.1f MHz", freqAt(0), freqAt(nPoints-1)));
}
void resizeVectors() {
	Gtk::Scale *s_freq,*s_time;
	GETWIDGET(s_freq); GETWIDGET(s_time);
	cal_X.resize(nPoints);
	cal_Y.resize(nPoints);
	cal_Z.resize(nPoints);
	cal_oc.resize(nPoints, 0);
	cal_sc.resize(nPoints, 0);
	cal_t.resize(nPoints, 0);
	cal_thru = vector<complex<double> >(nPoints, 1);
	s_freq->set_range(0, nPoints-1);
	s_time->set_range(0, timePoints()-1);
	polarView->points.resize(nPoints);
	for(int i=0;i<4;i++)
		graphView->lines[i].resize(nPoints);
	for(int i=0;i<2;i++)
		timeGraphView->lines[i].resize(timePoints());
}

int main(int argc, char** argv) {
	if(argc<2) {
		fprintf(stderr,"usage: %s /PATH/TO/TTY\n",argv[0]);
		return 1;
	}
	ttyFD=open(argv[1],O_RDWR);
	if(ttyFD<0) {
		perror("open");
		return 1;
	}
	
	struct termios tc;
	/* Set TTY mode. */
	if (tcgetattr(ttyFD, &tc) < 0) {
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
	tcsetattr(ttyFD, TCSANOW, &tc);
skip_tcsetaddr:
	
	// set up UI
	int argc1=1;
	auto app = Gtk::Application::create(argc1, argv, "org.gtkmm.example");
	string binDir=GetDirFromPath(GetProgramPath());
	if(chdir(binDir.c_str())<0)
		perror("chdir");

	builder = Gtk::Builder::create_from_string(string((char*)vna_glade, vna_glade_len));
	
	// controls
	Gtk::Window* window1;
	Gtk::Viewport *vp_main, *vp_graph, *vp_ttf;
	Gtk::Scale *s_freq, *s_time;
	
	// get controls
	GETWIDGET(window1); GETWIDGET(vp_main); GETWIDGET(vp_graph); GETWIDGET(vp_ttf); GETWIDGET(s_freq); GETWIDGET(s_time);
	addButtonHandlers();
	
	
	// polar view
	polarView = new xaxaxa::PolarView();
	vp_main->add(*polarView);
	polarView->show();
	
	// graph view
	graphView = new xaxaxa::GraphView();
	graphView->minValue = -40;
	graphView->maxValue = 50;
	graphView->hgridMin = -40;
	graphView->hgridSpacing = 10;
	graphView->selectedPoints = {0, 0, 0, 0};
	graphView->colors = {0x00aadd, 0xff8833, 0x0000ff, 0xff0000};
	graphView->lines.resize(4);
	vp_graph->add(*graphView);
	graphView->show();
	
	// time graph view
	timeGraphView = new xaxaxa::GraphView();
	timeGraphView->minValue = -60;
	timeGraphView->maxValue = 0;
	timeGraphView->hgridMin = -60;
	timeGraphView->hgridSpacing = 10;
	timeGraphView->selectedPoints = {0, 0};
	timeGraphView->colors = {0x0000ff, 0xff0000};
	timeGraphView->lines.resize(2);
	vp_ttf->add(*timeGraphView);
	timeGraphView->show();
	
	resizeVectors();
	
	// controls
	s_freq->set_value(0);
	s_time->set_value(0);
	s_freq->set_increments(1, 10);
	s_time->set_increments(1, 10);
	updateLabels();
	updateLabels_ttf();
	s_freq->signal_value_changed().connect([s_freq](){
		polarView->selectedPoint = (int)s_freq->get_value();
		graphView->selectedPoints[0] = graphView->selectedPoints[1]
			= graphView->selectedPoints[2] = graphView->selectedPoints[3] = (int)s_freq->get_value();
		updateLabels();
		polarView->queue_draw();
		graphView->queue_draw();
	});
	s_time->signal_value_changed().connect([s_time](){
		timeGraphView->selectedPoints[0] = timeGraphView->selectedPoints[1] = (int)s_time->get_value();
		updateLabels_ttf();
		timeGraphView->queue_draw();
	});
	
	// frequency dialog
	Gtk::Dialog* dialog_freq;
	Gtk::Entry *d_t_start, *d_t_step, *d_t_span;
	Gtk::Label *d_l_end;
	GETWIDGET(dialog_freq); GETWIDGET(d_l_end);
	GETWIDGET(d_t_start); GETWIDGET(d_t_step); GETWIDGET(d_t_span);
	
	auto func = [d_t_start, d_t_step, d_t_span, d_l_end](){
		auto start = d_t_start->get_text();
		auto step = d_t_step->get_text();
		auto span = d_t_span->get_text();
		double endFreq = atof(start.c_str()) + atof(step.c_str()) * (atoi(span.c_str()) - 1);
		d_l_end->set_text(ssprintf(20, "%.1f", endFreq));
	};
	d_t_start->signal_changed().connect(func);
	d_t_step->signal_changed().connect(func);
	d_t_span->signal_changed().connect(func);
	updateFreqButton();
	
	// periodic refresh
	sigc::connection conn = Glib::signal_timeout().connect([](){
		updateLabels();
		updateLabels_ttf();
		polarView->queue_draw();
		graphView->queue_draw();
		timeGraphView->queue_draw();
		return true;
	}, 200);
	
	
	if(pthread_create(&refreshThread, NULL, &thread1,NULL)<0) {
		perror("pthread_create");
		return 1;
	}

	return app->run(*window1);
}
