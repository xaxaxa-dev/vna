#include "include/xavna_cpp.H"
#include "include/xavna.h"
#include "include/platform_abstraction.H"
#include "include/workarounds.H"
#include <pthread.h>
#include <array>

using namespace std;

namespace xaxaxa {
    static void* _mainThread_(void* param);


	VNADevice::VNADevice() {
        _cb_ = &_cb;
        frequencyCompletedCallback2_ = [](int freqIndex, const vector<array<complex<double>, 4> >& values) {};
	}
	VNADevice::~VNADevice() {
		
	}
	vector<string> VNADevice::findDevices() {
		return xavna_find_devices();
	}
	
	void VNADevice::open(string dev) {
		if(_dev) close();
		if(dev == "") {
			auto tmp = findDevices();
			if(tmp.size() == 0) throw runtime_error("no vna device found");
			dev = tmp[0];
		}
		_dev = xavna_open(dev.c_str());
		if(!_dev) throw runtime_error(strerror(errno));
	}
	bool VNADevice::is_tr() {
		if(!_dev) return true;
		return xavna_is_tr(_dev);
	}
	void VNADevice::startScan() {
		if(!_dev) throw logic_error("VNADevice: you must call open() before calling startScan()");
		if(_threadRunning) return;
		_threadRunning = true;
		pthread_create(&_pth, NULL, &_mainThread_, this);
	}
	void VNADevice::stopScan() {
		if(!_threadRunning) return;
		_shouldExit = true;
		pthread_cancel(_pth);
		pthread_join(_pth, NULL);
		_shouldExit = false;
		_threadRunning = false;
	}
	void VNADevice::close() {
		if(_threadRunning) stopScan();
		if(_dev != NULL) {
			xavna_close(_dev);
			_dev = NULL;
		}
	}
	
	void VNADevice::takeMeasurement(function<void(const vector<VNARawValue>& vals)> cb) {
		if(!_threadRunning) throw logic_error("takeMeasurement: vna scan thread must be started");
		_cb = cb;
		__sync_synchronize();
		__sync_add_and_fetch(&_measurementCnt, 1);
	}
	
	void* VNADevice::_mainThread() {
		bool tr = is_tr();
		uint32_t last_measurementCnt = _measurementCnt;
		int cnt=0;
		while(!_shouldExit) {
			vector<VNARawValue> results(nPoints);
			for(int i=0;i<nPoints;i++) {
				fflush(stdout);
				int ports = tr?1:2;
				vector<array<complex<double>, 4> > values(ports);
				for(int port=0; port<ports; port++) {
                    if(xavna_set_params(_dev, (int)round(freqAt(i)/1000.),
                                        (port==0?attenuation1:attenuation2), port) < 0) {
						backgroundErrorCallback(runtime_error("xavna_set_params failed: " + string(strerror(errno))));
						return NULL;
					}
                    if(xavna_read_values_raw(_dev, (double*)&values[port], nValues)<0) {
						backgroundErrorCallback(runtime_error("xavna_read_values_raw failed: " + string(strerror(errno))));
						return NULL;
					}
				}
				VNARawValue tmp;
				if(tr) {
					if(disableReference)
						tmp << values[0][1], 0,
						        values[0][3], 0;
					else
						tmp << values[0][1]/values[0][0], 0,
						        values[0][3]/values[0][0], 0;
				} else {
					complex<double> a0,b0,a3,b3;
					complex<double> a0p,b0p,a3p,b3p;
					a0 = values[0][0];
					b0 = values[0][1];
					a3 = values[0][2];
					b3 = values[0][3];
					a0p = values[1][0];
					b0p = values[1][1];
					a3p = values[1][2];
					b3p = values[1][3];
					
					complex<double> d = 1. - (a3*a0p)/(a0*a3p);
					
					// S11M
					tmp(0,0) = ((b0/a0) - (b0p*a3)/(a3p*a0))/d;
					// S21M
					tmp(1,0) = ((b3/a0) - (b3p*a3)/(a3p*a0))/d;
					// S12M
					tmp(0,1) = ((b0p/a3p) - (b0*a0p)/(a3p*a0))/d;
					// S22M
					tmp(1,1) = ((b3p/a3p) - (b3*a0p)/(a3p*a0))/d;
				}
                frequencyCompletedCallback(i, tmp);
                frequencyCompletedCallback2_(i, values);
                
				results[i]=tmp;
				if(_shouldExit) return NULL;
			}
			sweepCompletedCallback(results);
			
			if(_measurementCnt != last_measurementCnt) {
				__sync_synchronize();
				if(cnt == 1) {
					function<void(const vector<VNARawValue>& vals)> func
						= *(function<void(const vector<VNARawValue>& vals)>*)_cb_;
					func(results);
					cnt = 0;
					last_measurementCnt = _measurementCnt;
				} else cnt++;
			}
			
		}
		return NULL;
	}
	static void* _mainThread_(void* param) {
		return ((VNADevice*)param)->_mainThread();
	}
}
