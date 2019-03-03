#include <thread>

int 
get_numcpus() 
{
	return std::thread::hardware_concurrency();
}

void usage() 
{
	printf("-c: control port\n");
}
