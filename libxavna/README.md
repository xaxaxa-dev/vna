# libxavna
C and C++ library for accessing the xaVNA hardware. There is only one library (.so or .dll), but several headers you can include, one for each api.

# C api
The C library allows low level access to the hardware; typical usage is:
1. Open the device
2. Set parameters (frequency, signal generator power, etc)
3. Read values
4. Repeat (2) and (3) as needed.
5. Close device

Note that the hardware is continuously sending data through the FIFO interface, so any time that you aren't reading it it is being queued up, meaning stale data can surface. The recommended usage pattern is to continuously read values in a loop, possibly in a background thread.
```c
#include <xavna/xavna.h>

// dev: path to the serial device
// returns: handle to the opened device, or NULL if failed; check errno
void* xavna_open(const char* dev);

// Set the RF frequency and attenuation.
// freq_khz: frequency in kHz
// atten1,atten2: attenuation in dB (positive integer) of signal generator on
// 				  port 1 and port 2 respectively; specify -1 to turn off signal gen
// returns: 0 if success; -1 if failure
int xavna_set_params(void* dev, int freq_khz, int atten1, int atten2);

// out_values: array of size 4 holding the following values:
//				reflection real, reflection imag,
//				thru real, thru imag
// n_samples: number of samples to average over; typical 50
// returns: number of samples read, or -1 if failure
int xavna_read_values(void* dev, double* out_values, int n_samples);

// out_values: array of size 8 holding the following values:
//				port 1 out real, port 1 out imag,
//				port 1 in real, port 1 in imag
//				port 2 out real, port 2 out imag,
//				port 2 in real, port 2 in imag
// n_samples: number of samples to average over; typical 50
// returns: number of samples read, or -1 if failure
int xavna_read_values_raw(void* dev, double* out_values, int n_samples);

// close device handle
void xavna_close(void* dev);
```

# C++ api
The C++ library provides a more convenient callback based interface, with frequency sweep and background data reading taken care of.
The typical usage is:
1. Instantiate VNADevice
2. Set callbacks and sweep parameters (start frequency, step frequency, number of points)
3. open()
4. startScan()
5. close()
6. delete VNADevice

```c++
#include <xavna/xavna_cpp.H>

namespace xaxaxa {
    class VNADevice {
    public:
        // frequency sweep parameters; do NOT change while background thread is running
        double startFreqHz=200e6;   // start frequency in Hz
        double stepFreqHz=25e6;     // step frequency in Hz
        int nPoints=50;             // how many frequency points
        int nValues=50;             // how many values to average over
        bool tr = true;         // whether to measure with only port 1 signal gen enabled
        
        // rf parameters
        int attenuation1, attenuation2;
        
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
        
        // return the frequency in Hz at an array index
        double freqAt(int i) {
            return startFreqHz+i*stepFreqHz;
        }
    };
}
```

