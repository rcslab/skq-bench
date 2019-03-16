#include <thread>
#include <time.h>

int 
get_numcpus() 
{
	return std::thread::hardware_concurrency();
}

void
usage() {
	printf("-p: connection port\n-c: control port\n");
}
