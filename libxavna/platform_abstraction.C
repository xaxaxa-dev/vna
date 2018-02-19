#include "include/platform_abstraction.H"
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include <stdexcept>

using namespace std;
vector<string> xavna_find_devices() {
	vector<string> ret;
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir ("/dev")) == NULL)
		throw runtime_error("xavna_find_devices: could not list /dev: " + 
							string(strerror(errno)));
	
	/* print all the files and directories within directory */
	while ((ent = readdir (dir)) != NULL) {
		string name = ent->d_name;
		if(name.find("ttyACM")==0)
			ret.push_back("/dev/"+name);
	}
	closedir (dir);
	return ret;
}
