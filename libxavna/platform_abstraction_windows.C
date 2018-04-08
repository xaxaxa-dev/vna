#include "include/platform_abstraction.H"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <windows.h>

using namespace std;
vector<string> xavna_find_devices() {
    vector<string> ret;
    for(int i=0;i<256;i++)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "\\\\.\\COM%i", i);

        HANDLE h = CreateFile(buf, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
        if(h == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            if(err == ERROR_SHARING_VIOLATION || err == ERROR_ACCESS_DENIED ||
               err == ERROR_GEN_FAILURE || err == ERROR_SEM_TIMEOUT) {
                ret.push_back(buf);
            }
        } else {
            ret.push_back(buf);
            CloseHandle(h);
        }
    }
    return ret;
}

int xavna_open_serial(const char* path) {
    HANDLE hComm;
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
    }
    DCB dcbSerialParams = { 0 }; // Initializing DCB structure
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hComm, &dcbSerialParams);
    SetCommState(hComm, &dcbSerialParams);
    
    return _open_osfhandle((intptr_t)hComm, 0);
}

void xavna_drainfd(int fd) {
    // TODO
}
