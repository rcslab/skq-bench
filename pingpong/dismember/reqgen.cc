#include "reqgen.h"
#include <ppd/msg.h>
#include "options.h"
#include <stdint.h>
#include <sstream>
#include <unistd.h>

////////////////
// TOUCH Generator
////////////////

touch_gen::touch_gen(const int conn_id, std::unordered_map<std::string, std::string>* args) : req_gen(conn_id)
{
    this->ugen = createGenerator("uniform:100");
	if (this->ugen == NULL) {
		E("Failed to create ugen for touch_gen\n");
	}

	if (args->find(PARAM_GEN) == args->end()) {
    	this->wgen = createGenerator(PARAM_GEN_DEFAULT);
	} else {
		this->wgen = createGenerator(args->at(PARAM_GEN));
	}

	if (this->wgen == NULL) {
		E("Failed to create wgen for touch_gen\n");
	}

	if (args->find(PARAM_UPDATE) == args->end()) {
		this->update_ratio = PARAM_UPDATE_DEFAULT;
	} else {
		this->update_ratio = atoi(args->at(PARAM_UPDATE).c_str());
	}
}

touch_gen::~touch_gen()
{
    delete wgen;
	delete ugen;
}

int touch_gen::send_req(int fd) 
{
    struct ppd_msg * msg;
	struct ppd_touch_arg load;

	msg = (struct ppd_msg *) malloc(sizeof(struct ppd_msg) + sizeof(struct ppd_touch_arg));

	if (msg == NULL) {
		E("Can't allocate memory for msg\n");
	}

	if (options.master_mode) {
		load.touch_cnt = 0;
	} else {
		load.touch_cnt = (uint32_t)this->wgen->generate();
	}

	if (this->ugen->generate() < this->update_ratio) {
		load.inc = 1;
	} else {
		load.inc = 0;
	}

	msg->cmd = PPD_CTOUCH;
	msg->dat_len = sizeof(struct ppd_touch_arg);
	memcpy(msg->dat, &load, sizeof(struct ppd_touch_arg));

    int err = write_msg(fd, msg);

    free(msg);

	return err;
}

int touch_gen::read_resp(int fd)
{
    struct ppd_msg * msg;

    if (read_msg(fd, &msg) < 0) {
		return -1;
	}

    if (msg->cmd != PPD_CRESP) {
        return -1;
    }

    return 0;
}

////////////////
// ECHO Generator
////////////////
int echo_gen::delay_table[DT_SZ];
std::atomic<int> echo_gen::delay_table_populated = ATOMIC_VAR_INIT(0);

void
echo_gen::populate_delay_table()
{
	int idx = 0;
	int expected = 0;

	// hack
	if (echo_gen::DT_SZ != 100) {
		E("Delay table size isn't 100");
	}

	/* 95 + 4 + 1 = 100 */
	if (!delay_table_populated.compare_exchange_weak(expected, 1)) {
		return;
	}

	delay_table[idx++] = 200;

	for(int i = 0; i < 4; i++) {
		delay_table[idx++] = 50;
	}

	for(int i = 0; i < 95; i++) {
		delay_table[idx++] = 10;
	}
}

echo_gen::echo_gen(const int conn_id, std::unordered_map<std::string, std::string>* args) : req_gen(conn_id)
{
	
	if (args->find(PARAM_GEN) == args->end()) {
		this->wgen = createGenerator(PARAM_GEN_DEFAULT);
	} else {
    	this->wgen = createGenerator(args->at(PARAM_GEN));
	}

	if (this->wgen == NULL) {
		E("Failed to create wgen for echo_gen");
	}
	
	if (args->find(PARAM_CDELAY) == args->end()) {
		this->cdelay = PARAM_CDELAY_DEFAULT;
	} else {
		this->cdelay = atoi(args->at(PARAM_CDELAY).c_str());
	}

	if (this->cdelay) {
		populate_delay_table();
	}
}

echo_gen::~echo_gen()
{
    delete wgen;
}

int echo_gen::get_delay()
{
	if (cdelay) {
		return delay_table[conn_id % DT_SZ];
	} else {
		return this->wgen->generate();
	}
}

int echo_gen::send_req(int fd) 
{
	struct ppd_msg *msg;
	struct ppd_echo_arg load;

	msg = (struct ppd_msg *) malloc(sizeof(struct ppd_msg) + sizeof(struct ppd_echo_arg));

	if (msg == NULL) {
		E("Can't allocate memory for msg\n");
	}

	if (options.master_mode) {
		load.enable_delay = 0;
	} else {
		load.enable_delay = get_delay();
	}

	msg->cmd = PPD_CECHO;
	msg->dat_len = sizeof(struct ppd_echo_arg);
	memcpy(msg->dat, &load, sizeof(struct ppd_echo_arg));

    int err = write_msg(fd, msg);

    free(msg);

	return err;
}

int echo_gen::read_resp(int fd)
{
    struct ppd_msg * msg;

    if (read_msg(fd, &msg) < 0) {
		return -1;
	}

    if (msg->cmd != PPD_CRESP) {
        return -1;
    }

    return 0;
}


////////////////
// HTTP Generator
////////////////
char http_gen::cons_buf[CONS_SZ];

http_gen::http_gen(const int conn_id, const std::string& host, std::unordered_map<std::string, std::string>* args) : req_gen(conn_id)
{
	// hack
	method = "GET";
	headers.insert({"Host", host});
	headers.insert({"Connection", "keep-alive"});
	major_ver = 1;
	minor_ver = 1;
	uri = "/";
}

http_gen::~http_gen()
{

}

std::string
http_gen::build_req()
{
	std::stringstream ss;

	ss << method << ' ' \
		<< uri << ' ' \
		<< "HTTP/" + std::to_string(major_ver) + "." + std::to_string(minor_ver) \
		<< "\r\n";

	for(auto &i : headers) {
		ss << i.first.c_str() << ": " << i.second.c_str() << "\r\n";
	}

	ss << "\r\n";

	return ss.str();
}

int http_gen::send_req(int fd) 
{
	std::string req = build_req();

	return writebuf(fd, req.c_str(), req.length());
}

int http_gen::read_resp(int fd)
{
	// hack
	// consume everything
	return read(fd, cons_buf, CONS_SZ);
}
