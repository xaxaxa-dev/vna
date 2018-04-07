#include "include/xavna_cpp.H"
#include "include/xavna.h"
#include "include/platform_abstraction.H"

#include <array>

using namespace std;

namespace xaxaxa {
    static void* _mainThread_(void* param);


	VNADevice::VNADevice() {
        _cb_ = &_cb;
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
	void VNADevice::startScan() {
		if(!_dev) throw logic_error("VNADevice: you must call open() before calling startScan()");
		if(_threadRunning) return;
		_threadRunning = true;
		pthread_create(&_pth, NULL, &_mainThread_, this);
	}
	void VNADevice::stopScan() {
		if(!_threadRunning) return;
		_shouldExit = true;
		pthread_join(_pth, NULL);
		_shouldExit = false;
		_threadRunning = false;
	}
	void VNADevice::close() {
		_ignoreError = true;
		if(_dev != NULL) {
			xavna_close(_dev);
			_dev = NULL;
		}
		if(_threadRunning) stopScan();
		_ignoreError = false;
	}
	
	void VNADevice::takeMeasurement(function<void(const vector<VNARawValue>& vals)> cb) {
		if(!_threadRunning) throw logic_error("takeMeasurement: vna scan thread must be started");
		_cb = cb;
		__sync_synchronize();
		__sync_add_and_fetch(&_measurementCnt, 1);
	}
	
	void* VNADevice::_mainThread() {
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
                                        (port==0?attenuation1:-1), (port==1?attenuation2:-1)) < 0) {
						if(_ignoreError) return NULL;
						backgroundErrorCallback(runtime_error("xavna_set_params failed: " + string(strerror(errno))));
						return NULL;
					}
                    if(xavna_read_values_raw(_dev, (double*)&values[port], nValues)<0) {
						if(_ignoreError) return NULL;
						backgroundErrorCallback(runtime_error("xavna_read_values_raw failed: " + string(strerror(errno))));
						return NULL;
					}
				}
				VNARawValue tmp;
				if(tr)
					tmp << values[0][1]/values[0][0], 0,
							values[0][3]/values[0][0], 0;
				else throw logic_error("not yet implemented");
                frequencyCompletedCallback(i, tmp);
                
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
