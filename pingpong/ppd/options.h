#pragma once

#include <vector>


static constexpr int MAX_MODE_PARAMS = 8;
static constexpr int MAX_MODE_PARAMS_LEN = 64;

struct server_option {
	int threads;
	int port;
	int skq;
	int skq_flag;
	int cpu_affinity;
	int skq_dump;
	int verbose;

	std::vector<char*> hpip;
	int kq_rtshare;
	int kq_tfreq;

	/* the mode this server runs in */
	int mode;

	char mode_params[MAX_MODE_PARAMS][MAX_MODE_PARAMS_LEN + 1];
	int num_mode_params;
};

extern server_option options;
