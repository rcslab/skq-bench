#include "reqproc.h"
#include <arpa/inet.h>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <random>
#include <stdint.h>
#include <sstream>
#include <sys/endian.h>
#include <sys/param.h>
#include <unistd.h>
#include "options.h"
#include "rocksdb/status.h"

#include <rocksdb/db.h>
#include <rocksdb/slice.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>

#include <msg.pb.h>
#include <util.h>
#include <const.h>

////////////////
// TOUCH Generator 
////////////////
touch_proc::touch_proc(std::unordered_map<std::string, std::string>* args) : req_proc()
{
	if (args->find(PARAM_TBSZ) != args->end()) {
		this->buffer_sz = atoi(args->at(PARAM_TBSZ).c_str());
	} else {
		this->buffer_sz = PARAM_TBSZ_DEFAULT;
	}
	
	V("Allocating %d items x %d CASZ for connnection\n", this->buffer_sz, CACHE_LINE_SIZE);
	this->buffer = (struct ppd_touch_cache_item *)aligned_alloc(CACHE_LINE_SIZE, this->buffer_sz * sizeof(struct ppd_touch_cache_item));

	this->rgen = new std::default_random_engine();
	this->rdist = new std::uniform_int_distribution<int>(0, INT32_MAX);
}

touch_proc::~touch_proc()
{
	delete this->buffer;
	delete this->rgen;
	delete this->rdist;
}

int touch_proc::proc_req(int fd)
{
	ppd_touch_req req;
	ppd_touch_resp resp;
	struct ppd_msg *msg = (struct ppd_msg *)this->read_buf;

	int ret = readmsg(fd, this->read_buf, this->MAX_READ_BUF_SIZE);

	if (ret < 0) {
		W("Readmsg failed with %d for connection %d\n", ret, fd);
		return ret;
	}

	if (!req.ParseFromArray(msg->payload, msg->size)) {
		W("ParseFromArray failed for connection %d\n", fd);
		return EINVAL; 
	}

	V("Conn %d touching %d items...\n", fd, req.touch_cnt());

	int sum = 0;
	int rand = (*this->rdist)(*this->rgen);
	for(int64_t i = rand; i < rand + req.touch_cnt(); i++) {
		if (req.inc() > 0) {
			this->buffer[i % this->buffer_sz].val += 1;
		} else {
			/* only read */
			sum += this->buffer[i % this->buffer_sz].val;
		}
	}

	resp.set_status(0);
	if (!resp.SerializeToArray(this->read_buf, MAX_READ_BUF_SIZE)) {
		W("Couldn't searialize to array connection %d\n", fd);
	}

	ret = writemsg(fd, this->read_buf, MAX_READ_BUF_SIZE, this->read_buf, resp.ByteSizeLong());
	if (ret < 0) {
		W("Writemsg failed with %d for connection %d\n", ret, fd);
	}

	return ret;
}

////////////////
// ECHO Generator
////////////////

echo_proc::echo_proc(std::unordered_map<std::string, std::string>*) : req_proc()
{
}

echo_proc::~echo_proc()
{
}

int echo_proc::proc_req(int fd) 
{
	ppd_echo_req req;
	ppd_echo_resp resp;
	struct ppd_msg *msg = (struct ppd_msg *)this->read_buf;
	int ret = readmsg(fd, this->read_buf, MAX_READ_BUF_SIZE);

	if (ret < 0) {
		W("Readbuf failed with %d for connection %d\n", ret, fd);
		return ret;
	}

	if (!req.ParseFromArray(msg->payload, msg->size)) {
		W("ParseFromArray failed for connection %d\n", fd);
		return EINVAL;
	}

	V("Connection %d delay %d us \n", fd, req.enable_delay());

	if (req.enable_delay() > 0) {
		uint64_t server_delay = req.enable_delay();
		uint64_t now = get_time_us();
		while (get_time_us() - now <= server_delay) {};
	}

	resp.set_status(0);
	if (!resp.SerializeToArray(this->read_buf, MAX_READ_BUF_SIZE)) {
		W("Couldn't searialize to array connection %d\n", fd);
	}

	ret = writemsg(fd, this->read_buf, MAX_READ_BUF_SIZE, this->read_buf, resp.ByteSizeLong());
	if (ret < 0) {
		W("Writemsg failed with %d for connection %d\n", ret, fd);
	}

	return ret;
}


////////////////
// rdb Generator
////////////////

rocksdb::DB * rdb_proc::db = nullptr;
std::atomic<int> rdb_proc::db_init {0};
char rdb_proc::DB_PATH_TEMP[] {"/tmp/rocksdbXXXXXX"};

rdb_proc::rdb_proc(std::unordered_map<std::string, std::string>*) : req_proc()
{
	int desired = 0;
	int target = 1;
	if (std::atomic_compare_exchange_strong(&rdb_proc::db_init, &desired, target)) {
		mktemp(DB_PATH_TEMP);

		V("Initializing rocksdb, path: %s.\n", DB_PATH_TEMP);
		
		rocksdb::Options opt;
		opt.use_direct_io_for_flush_and_compaction = USE_DIRECT_IO_FOR_FLUSH_AND_COMPACTION;
		opt.use_direct_reads = USE_DIRECT_READS;
		opt.OptimizeForPointLookup(CACHE_SIZE);
		opt.IncreaseParallelism();
		opt.OptimizeLevelStyleCompaction();
		opt.create_if_missing = true;

		rocksdb::Status s = rocksdb::DB::Open(opt, std::string(DB_PATH_TEMP), &this->db);
		if (!s.ok()) {
			E("Could not open rocksdb! Err %d\n", s.code()); 
		}

		rdb_proc::db_init.store(2);
		V("Finished initializing rocksdb.\n");
	} else {
		V("Checking for rocksdb initialization...\n");
		while(rdb_proc::db_init.load() != 2) {};
		V("Detected initialized rocksdb.\n");
	}
}

rdb_proc::~rdb_proc()
{
	delete this->db;
}

int rdb_proc::proc_req(int fd)
{
	ppd_rdb_resp resp;
	ppd_rdb_req req;
	rocksdb::Status s;
	struct ppd_msg *msg = (struct ppd_msg *)this->read_buf;

	int i = 0;
	int status = readmsg(fd, this->read_buf, MAX_READ_BUF_SIZE);

	if (status != 0) {
		W("Readmsg failed with %d for connection %d\n", status, fd);
		return status;
	}

	if (!req.ParseFromArray(msg->payload, msg->size)) {
		W("ParseFromArray failed for connection %d\n", fd);
		return EINVAL;
	}

	V("Connection %d op: %d, key: %s. val: %s. optarg: %d.\n", fd, req.op(), req.key().c_str(), req.val().c_str(), req.optarg());

	switch (req.op()) {
		case PPD_RDB_OP_PUT:{
			s = this->db->Put(rocksdb::WriteOptions(), req.key(), req.val());
			resp.set_status(s.code());
			break;
		}
		case PPD_RDB_OP_GET: {
			std::string val;
			s = this->db->Get(rocksdb::ReadOptions(), req.key(), &val);
			if (s.ok()) {
				resp.set_result(val);
			}
			resp.set_status(s.code());
			break;
		}
		case PPD_RDB_OP_SEEK: {
			rocksdb::Slice val;
			rocksdb::Iterator *it = this->db->NewIterator(rocksdb::ReadOptions(false, true));
			
			it->Seek(req.key());
			resp.set_status(it->status().code());

			if (it->Valid()) {
				val = it->value();
				resp.set_result(val.data(), val.size());
			}

			for(int64_t j = 0; j < req.optarg() && it->Valid(); j++) {
				rocksdb::Slice val = it->value();
				// do something about the key
				std::memcpy(this->read_buf, val.data(), MIN(val.size(), MAX_READ_BUF_SIZE));
				it->Next();
				if (!it->status().ok()) {
					resp.set_status(it->status().code());
					break;
				}
			}
			delete it;
			break;
		}
		default: {
			W("Invalid opcode %d for connection %d\n", req.op(), fd);
			return EINVAL;
		}
	}
	
	status = writemsg(fd, this->read_buf, MAX_READ_BUF_SIZE, this->read_buf, resp.ByteSizeLong());
	if (status < 0) {
		W("Writemsg failed with %d for connection %d\n", status, fd);
	}

	return status;
}
