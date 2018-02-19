#include <stdint.h>

// platform specific functions


int platform_init();

// bit-bang the jtag waveform;
// if progress is not NULL, it is kept updated with the current index of the playback
int platform_writeJtag(uint8_t* data, int len, volatile int* progress=0);

