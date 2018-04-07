#ifndef __XAVNA_CPP_H
#define __XAVNA_CPP_H
#include <pthread.h>
#include <complex>
#include <vector>
#include <stdexcept>
#include "common.H"

using namespace std;

namespace xaxaxa {
	class VNADevice {
	public:
		// frequency sweep parameters; do NOT change while background thread is running
		double startFreqHz=200e6;	// start frequency in Hz
		double stepFreqHz=25e6;		// step frequency in Hz
		int nPoints=50;				// how many frequency points
		int nValues=50;	 		// how many values to average over
		bool tr = true;			// whether to measure with only port 1 signal gen enabled
		
		// rf parameters
		int attenuation1=35, attenuation2=35;
		
		// called by background thread when a single frequency measurement is done
        function<void(int freqIndex, VNARawValue val)> frequencyCompletedCallback;
		
		// called by background thread when a complete sweep of all frequencies is done
		function<void(const vector<VNARawValue>& vals)> sweepCompletedCallback;
		
		// called by background thread when an error occurs
		function<void(const exception& exc)> backgroundErrorCallback;
		
		
		VNADevice();
		~VNADevice();
		
		// find all xaVNA devices present
		static vector<string> findDevices();
		
		// open a vna device
		// dev: path to the serial device; if empty, it will be selected automatically
		void open(string dev);
		
		// start the frequency sweep background thread, which repeatedly performs
		// scans until it is stopped using stopScan()
		void startScan();
		
		// stop the background thread
		void stopScan();
		
		// close the vna device
		void close();
		
		// wait for one full measurement, and call cb with results
		void takeMeasurement(function<void(const vector<VNARawValue>& vals)> cb);
		
		// return the frequency in Hz at an array index
		double freqAt(int i) {
			return startFreqHz+i*stepFreqHz;
		}
		
		// return highest power output in dBm
		int maxPower() const {
			return 10;
		}
		
		
		
		// internal variables
		void* _dev=NULL;
		pthread_t _pth;
		bool _threadRunning=false;
		bool _shouldExit=false;
		bool _ignoreError=false;
		volatile uint32_t _measurementCnt=0;
		function<void(const vector<VNARawValue>& vals)> _cb;
		volatile function<void(const vector<VNARawValue>& vals)>* _cb_;
		
		// internal methods
		void* _mainThread();
	};
}
#endif
