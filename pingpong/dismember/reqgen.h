#ifndef REQGEN_H
#define REQGEN_H

#include <string>
#include <unordered_map>
#include "Generator.h"
#include "options.h"

#define DISABLE_EVIL_CONSTRUCTORS(name) \
	name(const name&) = delete; \
	void operator=(const name) = delete

class req_gen {
	protected:
		const int conn_id;
	public:
		req_gen(const int id) : conn_id(id) {}; 
		virtual ~req_gen() {};
		virtual int send_req(int fd) = 0;
		virtual int read_resp(int fd) = 0;
};

class touch_gen : public req_gen
{
	private:
		static constexpr const char* PARAM_GEN = "GEN";
		static constexpr const char* PARAM_GEN_DEFAULT = "fixed:64";
		static constexpr const char* PARAM_UPDATE = "UPDATE";
		static constexpr const int PARAM_UPDATE_DEFAULT = 0;
		Generator *wgen;
		Generator *ugen;
		int update_ratio;
	public:
		touch_gen(const int conn_id, std::unordered_map<std::string, std::string>* args);
		touch_gen() = delete;
		~touch_gen();
		DISABLE_EVIL_CONSTRUCTORS(touch_gen);
	 	int send_req(int fd);
		int read_resp(int fd);
};

class echo_gen : public req_gen
{
	private:
		static constexpr const char* PARAM_GEN = "GEN";
		static constexpr const char* PARAM_GEN_DEFAULT = "fixed:0";
		static constexpr const char* PARAM_CDELAY = "CDELAY";
		static constexpr const int PARAM_CDELAY_DEFAULT = 0;
		static constexpr const int DT_SZ = 100;
		static int delay_table[DT_SZ];
		static void populate_delay_table();
		static std::atomic<int> delay_table_populated;

		Generator *wgen;
		int cdelay;
		int get_delay();
	public:
		echo_gen(const int conn_id, std::unordered_map<std::string, std::string>* args);
		echo_gen() = delete;
		~echo_gen();
		DISABLE_EVIL_CONSTRUCTORS(echo_gen);
	 	int send_req(int fd);
		int read_resp(int fd);
};

class http_gen : public req_gen
{
	private:
		std::string build_req();
		std::string method;
		std::unordered_map<std::string, std::string> headers;
		std::string uri;
		int major_ver;
		int minor_ver;
		static constexpr const int CONS_SZ = 1024 * 1024 * 4;
		static char cons_buf[CONS_SZ];
	public:
		http_gen(const int conn_id, const std::string& host, std::unordered_map<std::string, std::string>* args);
		http_gen() = delete;
		~http_gen();
		DISABLE_EVIL_CONSTRUCTORS(http_gen);
	 	int send_req(int fd);
		int read_resp(int fd);
};

#endif