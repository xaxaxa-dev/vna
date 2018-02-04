
extern "C" {
	// dev: path to the serial device; if NULL, it will be selected automatically
	// returns: handle to the opened device, or NULL if failed; check errno
	void* xavna_open(const char* dev);

	void* xavna_get_chained_device(void* dev);

	// Set the RF frequency and attenuation.
	// freq_khz: frequency in kHz
	// atten: attenuation in dB (positive integer)
	// returns: 0 if success; -1 if failure
	int xavna_set_params(void* dev, int freq_khz, int atten);

	// out_values: array of size 4 holding the following values:
	//				reflection real, reflection imag,
	//				thru real, thru imag
	// n_samples: number of samples to average over; typical 50
	// returns: number of samples read, or -1 if failure
	int xavna_read_values(void* dev, double* out_values, int n_samples);

	// close device handle
	void xavna_close(void* dev);

}
