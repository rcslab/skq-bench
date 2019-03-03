#include <stdio.h>
#include <string.h>
#include <netdb.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils.h"

void
usage() 
{
	printf("Usage:\n-s: server addr.\n-P: server ctl port.\n-i: c/s conn port.\n-c: client addr.\n-p: client ctl port.\n-n: number of clients.\n-f: test script name.\n-o: test output file name.\n");
}

std::string 
get_ip_from_hostname(std::string hostname) 
{
	static char rt[100];
	struct in_addr **addr;
	struct hostent *he;

	if ((he = gethostbyname(hostname.c_str())) == NULL) {
		printf("Hostname %s cannot be resolved.\n", hostname.c_str());
		abort();
	}
	addr = (struct in_addr**)he->h_addr_list;
	for (int i=0;addr[i]!=NULL;i++) {
		strcpy(rt, inet_ntoa(*addr[i]));
		return rt;
	}
	return rt;
}
