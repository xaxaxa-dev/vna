#include "include/platform_abstraction.H"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;
vector<string> xavna_find_devices() {
	vector<string> ret;
	// TODO
	return ret;
}

int xavna_open_serial(const char* path) {
	/*HANDLE hComm;
	hComm = CreateFile(path,                //port name
				  GENERIC_READ | GENERIC_WRITE, //Read/Write
				  0,                            // No Sharing
				  NULL,                         // No Security
				  OPEN_EXISTING,// Open existing port only
				  0,            // Non Overlapped I/O
				  NULL);        // Null for Comm Devices

	if (hComm == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Error in opening serial port");
		return -1;
	}*/
	
	return open(path,O_RDWR);
}

void xavna_drainfd(int fd) {
	// TODO
}
