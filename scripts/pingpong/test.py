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

from libtc import *

step_inc_pct = 100
init_step = 100000

term_pct = 5
inc_pct = 50

# paths
test_dir = "/tmp/tests.d/"
file_dir = os.path.dirname(os.path.realpath(__file__))
root_dir = file_dir + "/../../"
sample_filename = "sample.txt"

sched = [
	"vanilla", -1,
	"queue0", tc_make_sched_flag(SCHED_QUEUE, 0),
	"queue1", tc_make_sched_flag(SCHED_QUEUE, 1),
    "queue2", tc_make_sched_flag(SCHED_QUEUE, 2),
	"q0_ws", tc_make_sched_flag(SCHED_QUEUE, 0, feat=SCHED_FEAT_WS, fargs=1),
	"q1_ws", tc_make_sched_flag(SCHED_QUEUE, 1, feat=SCHED_FEAT_WS, fargs=1),
	"q2_ws", tc_make_sched_flag(SCHED_QUEUE, 2, feat=SCHED_FEAT_WS, fargs=1),
	"cpu0_ws", tc_make_sched_flag(SCHED_CPU, 0, feat=SCHED_FEAT_WS, fargs=1),
    "cpu1_ws", tc_make_sched_flag(SCHED_CPU, 1, feat=SCHED_FEAT_WS, fargs=1),
    "cpu2_ws", tc_make_sched_flag(SCHED_CPU, 2, feat=SCHED_FEAT_WS, fargs=1),
    "best2", tc_make_sched_flag(SCHED_BEST, 2),
	"cpu0", tc_make_sched_flag(SCHED_CPU, 0),
	"cpu1", tc_make_sched_flag(SCHED_CPU, 1),
	"cpu2", tc_make_sched_flag(SCHED_CPU, 2),
]

master = ["skylake2"]
server = ["skylake1"]
clients = ["skylake3", "skylake4", "skylake5", "skylake6", "skylake7", "skylake8"]

threads = 12
client_threads = 12
warmup = 5
duration = 10
cooldown = 0
conn_per_thread = 12


hostfile = None
dump = False
lockstat = False
client_only = False

def get_cpu_str(threads):
	ret = "cpuset -l 0-23" # + str(threads - 1)
	return ret

def stop_all():
	# stop clients
	tc_log_print("Stopping clients...")
	tc_remote_exec(clients, "killall -9 dismember", check=False)

	if not client_only:
		# stop server
		tc_log_print("Stopping server...")
		tc_remote_exec(server, "killall -9 ppd", check=False)

	# stop master
	tc_log_print("Stopping master...")
	tc_remote_exec(master, "killall -9 dismember", check=False)

def get_client_str(clt):
	ret = " "
	for client in clt:
		ret += " -a " + client + " "
	return ret

def parse_file(f):
	qps = []
	lat = []
	lines = f.readlines()
	for line in lines:
		entry = line.split()
		if len(entry) != 2:
			raise Exception("Unrecognized line: " + line)
		qps.append(float(entry[0]))
		lat.append(float(entry[1]))
	return qps, lat

def run_exp(sc, ld, lstat):
	while True:
		if client_only:
			ssrv = None
		else:
			# start server
			tc_log_print("Starting server...")
			server_cmd = test_dir + "/pingpong/ppd/ppd -a -D -t " + str(threads)
			
			if lstat:
				server_cmd = "sudo lockstat -A -P -s4 -n16777216 " + server_cmd
			
			if sc != -1:
				server_cmd = server_cmd + " -m " + str(sc)
				if dump:
					server_cmd += " -d 1 "

			tc_log_print(server_cmd)

			ssrv = tc_remote_exec(server, server_cmd, blocking=False)

		# start clients
		tc_log_print("Starting clients...")
		client_cmd = get_cpu_str(client_threads) + " " + test_dir + "/pingpong/dismember/dismember -A"
		tc_log_print(client_cmd)
		sclt = tc_remote_exec(clients, client_cmd, blocking=False)

		time.sleep(1)
		# start master
		tc_log_print("Starting master...")
		master_cmd = get_cpu_str(client_threads) + " " + test_dir + "/pingpong/dismember/dismember " + \
			                  get_client_str(clients) + \
							  " -s " + server[0] + \
							  " -q " + str(ld) + \
							  " -c " + str(conn_per_thread) + \
							  " -o " + test_dir + "/" + sample_filename + \
							  " -t " + str(client_threads) + \
							  " -T " + str(client_threads) + \
							  " -C 1 " + \
							  " -Q 1000 " + \
							  " -w " + str(duration) + \
							  " -W " + str(warmup)

		tc_log_print(master_cmd)
		sp = tc_remote_exec(master, master_cmd, blocking=False)
		p = sp[0]

		success = False
		cur = 0
		while True:
			# either failed or timeout
			# we use failure detection to save time for long durations
			if False \
				or not tc_scan_stderr(sp, exclude=[".*warn.*", ".*WARN.*", ".*DEBUG.*"]) \
				or not tc_scan_stderr(ssrv, exclude=[".*warn.*", ".*WARN.*", ".*DEBUG.*"]) \
				or not tc_scan_stderr(sclt, exclude=[".*warn.*", ".*WARN.*", ".*DEBUG.*"]) \
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

# generate multilate memparse format
def build_memparse(lat_arr, qps_arr):

	output = '{0: <10}'.format('#type') + '{0: >8}'.format('avg') + '{0: >8}'.format('std') + \
				      '{0: >8}'.format('min') + '{0: >8}'.format('5th') + '{0: >8}'.format('10th') + \
					  '{0: >8}'.format('50th') + '{0: >8}'.format('90th')  + '{0: >8}'.format('95th') + '{0: >8}'.format('99th') + "\n"
	
	output += '{0: <10}'.format('read') + '{0: >8}'.format("{:.1f}".format(np.mean(lat_arr))) + '{0: >8}'.format("{:.1f}".format(np.std(lat_arr))) + \
				      '{0: >8}'.format("{:.1f}".format(np.min(lat_arr))) + \
					  '{0: >8}'.format("{:.1f}".format(np.percentile(lat_arr, 5))) + \
					  '{0: >8}'.format("{:.1f}".format(np.percentile(lat_arr, 10))) + \
					  '{0: >8}'.format("{:.1f}".format(np.percentile(lat_arr, 50))) + \
					  '{0: >8}'.format("{:.1f}".format(np.percentile(lat_arr, 90))) + \
					  '{0: >8}'.format("{:.1f}".format(np.percentile(lat_arr, 95))) + \
					  '{0: >8}'.format("{:.1f}".format(np.percentile(lat_arr, 99))) + "\n" \

	output += "\n" + "Total QPS = " + "{:.1f}".format(np.mean(qps_arr)) + " (0 / 0s)"

	return output

def keep_results(output, sout, serr):
	scpcmd = "scp " + master[0] + ":" + test_dir + "/" + sample_filename + " " + tc_get_odir() + "/sample.txt"
	tc_log_print(scpcmd)
	sp.check_call(scpcmd, shell=True)

	# parse the output file for percentile and average load
	f = open(tc_get_odir() + "/sample.txt", "r")
	qps, lat = parse_file(f)
	f.close()
	avg_qps = np.mean(qps)
	
	output = build_memparse(lat, qps)
	f = open(tc_get_odir() + "/l" + str(int(avg_qps)) + ".txt", "w")
	f.write(output)
	f.close()

	tc_log_print(output)

	mvcmd = "mv " + tc_get_odir() + "/sample.txt " + tc_get_odir() + "/l" + str(int(avg_qps)) + ".sample"
	tc_log_print(mvcmd)
	sp.check_call(mvcmd, shell=True)

	if lockstat and len(serr) > 0:
		f = open(tc_get_odir() + "/l" + str(int(avg_qps))  + ".lstat", "w")
		f.write(serr)
		f.close()
	
	if dump and len (sout) > 0:
		f = open(tc_get_odir() + "/l" + str(int(avg_qps)) + ".kstat", "w")
		f.write(sout)
		f.close()

	return int(avg_qps)

def main():
	global hostfile
	global server
	global clients
	global dump
	global lockstat
	global client_only

	options = getopt.getopt(sys.argv[1:], 'h:sldc')[0]
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

	tc_init(str(threads) + "+" + str(len(clients)) + "x" + str(client_threads) + "x" + str(conn_per_thread))

	tc_log_print("Configuration:\n" + \
		  "Hostfile: " + ("None" if hostfile == None else hostfile) + "\n" \
		  "Lockstat: " + str(lockstat) + "\n" \
		  "KQ dump: " + str(dump) + "\n" \
		  "Client only: " + str(client_only) + "\n")

	if hostfile != None:
		hosts = tc_parse_hostfile(hostfile)
		server = tc_process_hostnames(server, hosts)
		clients = tc_process_hostnames(clients, hosts)
		master = tc_process_hostnames(master, hosts)

	stop_all()

	for i in range(0, len(sched), 2):
		esched = sched[i+1]
		ename = sched[i]
		step_mul = 100
		last_load = 0
		cur_load = init_step

		tc_begin(ename)

		while True:
			tc_log_print("============ Sched: " + str(ename) + " Flag: " + format(esched, '#04x') + " Load: " + str(cur_load) + " ============")

			output, sout, serr = run_exp(esched, cur_load, lockstat)

			qps = keep_results(output, sout, serr)
			
			pct = int((qps - last_load) / init_step * 100)
			tc_log_print("last_load: " + str(last_load) + " this_load: " + str(qps) + " inc_pct: " + str(pct) + "%")

			if pct <= term_pct:
				tc_log_print("inc_pct less than TERM_PCT " + str(term_pct) + "%. Done.")
				break

			if pct <= inc_pct:
				step_mul += step_inc_pct
				tc_log_print("inc_pct less than INC_PCT " + str(inc_pct) + "%. Increasing step multiplier to " + str(step_mul) + "%")

			last_load = qps
			cur_load += int(init_step * step_mul / 100)
			tc_log_print("")
		
		tc_end()

	stop_all()

main()
