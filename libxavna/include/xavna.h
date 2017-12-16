
extern "C" {
	// dev: path to the serial device; if NULL, it will be selected automatically
	// returns: handle to the opened device, or NULL if failed; check errno
	void* xavna_open(const char* dev);

	void* xavna_get_chained_device(void* dev);

	// Set the RF frequency.
	// freq_khz: frequency in kHz
	// returns: actual frequency set, in kHz; -1 if failure
	int xavna_set_frequency(void* dev, int freq_khz);

	// Set the signal generator attenuation
	// a: attenuation in dB
	// returns: 0 if success; -1 if failure
	int xavna_set_attenuation(void* dev, int a);

	// Set whether the signal source is enabled.
	// en: 0 to disable, nonzero to enable
	// returns: 0 if success, -1 if failure
	int xavna_set_source_enabled(void* dev, int en);


	// out_values: array of size 4 holding the following values:
	//				reflection real, reflection imag,
	//				thru real, thru imag
	// n_samples: number of samples to average over; typical 50
	// returns: number of samples read, or -1 if failure
	int xavna_read_values(void* dev, double* out_values, int n_samples);

	// close device handle
	void xavna_close(void* dev);

}
