#include <thread>

int 
get_numcpus() 
{
	return std::thread::hardware_concurrency();
}

void usage() 
{
	printf("-i: ip address\n-m: mgr ip address\n-p: port\n-c: control port\n");
}
