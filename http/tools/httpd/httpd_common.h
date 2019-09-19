#ifndef __CELESTIS_HTTPD_COMMON_H__
#define __CELESTIS_HTTPD_COMMON_H__

#include <sys/types.h>
#include <sys/stat.h>

#include <string>
#include <thread>

class Server
{
public:
	int port, kq_num;
	int m_kq_mode = 0;
	bool m_kq = false;
	bool s_lb = false; 
	std::string name;
	std::string address;
	std::string root;
	std::vector<std::string> index;

	Server(int p, std::string n, std::string a, std::string r) :
		port(p), name(n), address(a), root(r) {};
	~Server() {};
};

int
is_path_point_to_file(const std::string& path)
{
	 //struct stat path_stat;
     //stat(path.c_str(), &path_stat);
	 //return S_ISREG(path_stat.st_mode);
	 if (path[path.length()-1] == '/') {
		 return 0;
	 }
	 
	 return 1;
}

int 
get_numcpus() 
{
	 return std::thread::hardware_concurrency();
}

#endif
