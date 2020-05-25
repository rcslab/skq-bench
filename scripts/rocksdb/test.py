#!/usr/bin/env python3.6
import subprocess as sp
import time
import select
import os
import datetime
import pwd
import sys
import getopt
import numpy as np
import re

import memparse as mp
import libtc as tc

step_inc_pct = 100
init_step = 20000 #
start_step = 10000
term_qps = 85000000000

term_pct = 1
inc_pct = 50
server_port = 23444

# paths
test_dir = "/tmp/tests.d/"
file_dir = os.path.dirname(os.path.realpath(__file__))
root_dir = file_dir + "/../../"
sample_filename = "sample.txt"

sched = [
	#"vanilla", -1,
	#"cpu0", tc.make_sched_flag(tc.SCHED_CPU, 0),
	"queue0", tc.make_sched_flag(tc.SCHED_QUEUE, 0),
	#"cpu0_ws2", tc.make_sched_flag(tc.SCHED_CPU, 0, feat=tc.SCHED_FEAT_WS, fargs=1),
    #"cpu2_ws2", tc.make_sched_flag(tc.SCHED_CPU, 2, feat=tc.SCHED_FEAT_WS, fargs=1),
	#"best2", tc.make_sched_flag(tc.SCHED_BEST, 2),
	#"best2_ws2", tc.make_sched_flag(tc.SCHED_BEST, 2, feat=tc.SCHED_FEAT_WS, fargs=1),
	#"q0_ws2", tc.make_sched_flag(tc.SCHED_QUEUE, 0, feat=tc.SCHED_FEAT_WS, fargs=1),
	#"queue2", tc.make_sched_flag(tc.SCHED_QUEUE, 2),
	#"q2_ws2", tc.make_sched_flag(tc.SCHED_QUEUE, 2, feat=tc.SCHED_FEAT_WS, fargs=1),
	#"cpu0_ws2", tc.make_sched_flag(tc.SCHED_CPU, 0, feat=tc.SCHED_FEAT_WS, fargs=1),
	#"cpu2", tc.make_sched_flag(tc.SCHED_CPU, 2),
	#"rand", make_sched_flag(0, 0),
]

master = ["skylake2"]
ssh_master = master.copy()

server = ["skylake1"]
ssh_server = server.copy()

clients = ["skylake3", "skylake5", "skylake6", "skylake7", "skylake8", "sandybridge1", "sandybridge2", "sandybridge3", "sandybridge4"]
ssh_clients = clients.copy()

threads = 12
client_threads = threads
warmup = 5
duration = 10
cooldown = 0
cacheline = 0
conn_per_thread = 12
conn_delay = False
db_path = "/mnt/md/rocksdb.db"
#db_path = "/home/oscar/rocksdb.db"

hostfile = None
dump = True
lockstat = False
client_only = False

def stop_all():
	# stop clients
	tc.log_print("Stopping clients...")
	tc.remote_exec(ssh_clients, "sudo killall -9 dismember", check=False)

	if not client_only:
		# stop server
		tc.log_print("Stopping server...")
		tc.remote_exec(ssh_server, "sudo killall -9 ppd", check=False)

		# stop master
		tc.log_print("Stopping master...")
		tc.remote_exec(ssh_master, "sudo killall -9 dismember", check=False)

def get_client_str(clt):
	ret = " "
	for client in clt:
		ret += " -a " + client + " "
	return ret

def run_exp(sc, ld, lstat):
	while True:
		if client_only:
			ssrv = None
		else:
			# start server
			tc.log_print("Starting server...")
			server_cmd = "cpuset -l 0-23 " + test_dir + "/pingpong/build/ppd -a -t " + str(threads) + " -p " + str(server_port) + " -M 2 -OPATH=" + db_path

			if lstat:
				server_cmd = "sudo lockstat -A -P -n65536 " + server_cmd

			if sc != -1:
				server_cmd = server_cmd + " -m " + str(sc)
				if dump:
					server_cmd += " -d 1 "
			tc.log_print(server_cmd)

			ssrv = tc.remote_exec(ssh_server, server_cmd, blocking=False)

		# start clients
		tc.log_print("Starting clients...")
		client_cmd = tc.get_cpuset_core(client_threads) + " " + test_dir + "/pingpong/build/dismember -A"
		tc.log_print(client_cmd)
		sclt = tc.remote_exec(ssh_clients, client_cmd, blocking=False)

		time.sleep(1)
		# start master
		tc.log_print("Starting master...")
		master_cmd = tc.get_cpuset_core(client_threads) + " " + test_dir + "/pingpong/build/dismember " + \
			                  get_client_str(clients) + \
							  " -s " + server[0] + \
							  " -p " + str(server_port) + \
							  " -q " + str(ld) + \
							  " -c " + str(conn_per_thread) + \
							  " -o " + test_dir + "/" + sample_filename + \
							  " -t " + str(client_threads) + \
							  " -w " + str(duration) + \
							  " -W " + str(warmup) + \
                              " -T " + str(client_threads) + \
							  " -i fb_ia " + \
							  " -C 1 " + \
							  " -Q 1000 " + \
							  " -l 2 "

		tc.log_print(master_cmd)
		sp = tc.remote_exec(ssh_master, master_cmd, blocking=False)
		p = sp[0]

		success = False
		cur = 0
		while True:
			# either failed or timeout
			# we use failure detection to save time for long durations
			if False \
				or not tc.scan_stderr(sp, exclude=[".*warn.*", ".*WARN.*", ".*DEBUG.*"]) \
				or not tc.scan_stderr(ssrv, exclude=[".*warn.*", ".*WARN.*", ".*DEBUG.*"]) \
				or not tc.scan_stderr(sclt, exclude=[".*warn.*", ".*WARN.*", ".*DEBUG.*"]) \
				or cur >= int(warmup + duration) * 2 \
					:
				break
			
			if p.poll() != None:
				success = True
				break

			time.sleep(1)
			cur = cur + 1

		stop_all()

		print("Cooling down...")
		time.sleep(cooldown)
		
		output = p.communicate()[0].decode(sys.getfilesystemencoding())
		if not client_only:
			stdout, stderr = ssrv[0].communicate()
			stdout = stdout.decode(sys.getfilesystemencoding())
			stderr = stderr.decode(sys.getfilesystemencoding())
		else:
			stdout = ""
			stderr = ""
			
		if success:
			# return mout, sout, serr (master output, server output, server serr)
			return output, stdout, stderr

def keep_results(output, sout, serr):
	scpcmd = "scp " + ssh_master[0] + ":" + test_dir + "/" + sample_filename + " " + tc.get_odir() + "/sample.txt"
	tc.log_print(scpcmd)
	sp.check_call(scpcmd, shell=True)

	# parse the output file for percentile and average load
	qps, lat = mp.parse_mut_sample(tc.get_odir() + "/sample.txt")
	avg_qps = np.mean(qps)
	
	output = mp.build_mut_output(lat, qps)
	f = open(tc.get_odir() + "/l" + str(int(avg_qps)) + ".txt", "w")
	f.write(output)
	f.close()

	tc.log_print(output)

	mvcmd = "mv " + tc.get_odir() + "/sample.txt " + tc.get_odir() + "/l" + str(int(avg_qps)) + ".sample"
	tc.log_print(mvcmd)
	sp.check_call(mvcmd, shell=True)

	if lockstat and len(serr) > 0:
		f = open(tc.get_odir() + "/l" + str(int(avg_qps))  + ".lstat", "w")
		f.write(serr)
		f.close()
	
	if dump and len (sout) > 0:
		f = open(tc.get_odir() + "/l" + str(int(avg_qps)) + ".kstat", "w")
		f.write(sout)
		f.close()

	return int(avg_qps)

def main():
	global hostfile
	global server
	global master
	global clients
	global dump
	global lockstat
	global client_only

	options = getopt.getopt(sys.argv[1:], 'h:sldcp')[0]
	for opt, arg in options:
		if opt in ('-h'):
			hostfile = arg
		elif opt in ('-s'):
			stop_all()
			return
		elif opt in ('-l'):
			lockstat=True
		elif opt in ('-d'):
			dump=True
		elif opt in ('-c'):
			client_only=True

	tc.init("~/results.d/rocksdb/" + str(threads) + "+" + str(len(clients)) + "x" + str(client_threads) + "x" + str(conn_per_thread))

	tc.log_print("Configuration:\n" + \
		  "Hostfile: " + ("None" if hostfile == None else hostfile) + "\n" \
		  "Lockstat: " + str(lockstat) + "\n" \
		  "KQ dump: " + str(dump) + "\n" \
		  "Client only: " + str(client_only) + "\n" + \
		  "Conn delay: " + str(conn_delay) + "\n")

	if hostfile != None:
		hosts = tc.parse_hostfile(hostfile)
		server = tc.process_hostnames(server, hosts)
		clients = tc.process_hostnames(clients, hosts)
		master = tc.process_hostnames(master, hosts)

	stop_all()

	for i in range(0, len(sched), 2):
		esched = sched[i+1]
		ename = sched[i]
		step_mul = 100
		last_load = 0
		cur_load = start_step

		tc.begin(ename)

		tc.log_print("============ Sched: " + str(ename) + " Flag: " + format(esched, '#04x') + " Load: MAX" + " ============")
		output, sout, serr = run_exp(esched, 0, lockstat)
		keep_results(output, sout, serr)
		stop_all()
		
		while True:
			tc.log_print("============ Sched: " + str(ename) + " Flag: " + format(esched, '#04x') + " Load: " + str(cur_load) + " ============")

			output, sout, serr = run_exp(esched, cur_load, lockstat)

			qps = keep_results(output, sout, serr)
			
			pct = int((qps - last_load) / init_step * 100)
			tc.log_print("last_load: " + str(last_load) + " this_load: " + str(qps) + " inc_pct: " + str(pct) + "%")

			if cur_load > term_qps:
				tc.log_print("qps more than " + str(term_qps) + "%. Done.")
				break

			if pct <= term_pct:
				tc.log_print("inc_pct less than TERM_PCT " + str(term_pct) + "%. Done.")
				break

			if pct <= inc_pct:
				step_mul += step_inc_pct
				tc.log_print("inc_pct less than INC_PCT " + str(inc_pct) + "%. Increasing step multiplier to " + str(step_mul) + "%")

			last_load = qps
			cur_load += int(init_step * step_mul / 100)
			tc.log_print("")
		
		tc.end()

	stop_all()

main()
