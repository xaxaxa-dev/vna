# libxavna
C and C++ libraries for accessing the xaVNA hardware.

# C library
The C library allows low level access to the hardware; typical usage is:
1. Open the device
2. Set parameters (frequency, signal generator power, etc)
3. Read values
4. Repeat (2) and (3) as needed.

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

