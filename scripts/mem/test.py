#!/usr/bin/env python3.6
import subprocess as sp
import time
import select
import os
import datetime
import pwd
import sys
import getopt
import re

import memparse as mp
import libtc as tc

# paths
test_dir = "/tmp/tests.d"
file_dir = os.path.dirname(os.path.realpath(__file__))
root_dir = file_dir + "/../../"

sample_filename = "sample.txt"

sched = [
 	#"linox", -2,
	#"arachne", -3,
	"vanilla", -1,
	"queue0", tc.make_sched_flag(tc.SCHED_QUEUE, 0),
    "cpu0", tc.make_sched_flag(tc.SCHED_CPU, 0),
	#"best2", tc.make_sched_flag(tc.SCHED_BEST, 2),
	#"queue2", tc.make_sched_flag(tc.SCHED_QUEUE, 2),
	#"cpu2", tc.make_sched_flag(tc.SCHED_CPU, 2),
	"q0_ws", tc.make_sched_flag(tc.SCHED_QUEUE, 0, feat=tc.SCHED_FEAT_WS, fargs=1),
	#"q2_ws", tc.make_sched_flag(tc.SCHED_QUEUE, 2, feat=tc.SCHED_FEAT_WS, fargs=1),
	"cpu0_ws", tc.make_sched_flag(tc.SCHED_CPU, 0, feat=tc.SCHED_FEAT_WS, fargs=1),
    #"cpu2_ws", tc.make_sched_flag(tc.SCHED_CPU, 2, feat=tc.SCHED_FEAT_WS, fargs=1),
	#"best2_ws", tc.make_sched_flag(tc.SCHED_BEST, 2, feat=tc.SCHED_FEAT_WS, fargs=1),
	#"rand", make_sched_flag(0, 0),
]

step_inc_pct = 100
init_step = 50000

term_pct = 1
inc_pct = 50

master = ["skylake2"]
master_ssh = master.copy()
server = ["skylake1"]
server_ssh = server.copy()
clients = ["skylake3", "skylake4", "skylake5", "skylake6", "skylake7", "skylake8", "sandybridge1",  "sandybridge2",  "sandybridge3", "sandybridge4"]
clients_ssh = clients.copy()

threads = 12
client_threads = 12
warmup = 5
duration = 10
cooldown = 0
conn_per_thread = 12
hostfile = None
dump = True
lockstat = False
client_only = False

def get_client_str(cl):
	ret = ""
	for client in cl:
		ret += " -a " + client
	return ret

def stop_all():
	# stop clients
	tc.log_print("Stopping clients...")
	tc.remote_exec(clients_ssh, "killall -9 mutilate", check=False)

	if not client_only:
		# stop server
		tc.log_print("Stopping server...")
		tc.remote_exec(server_ssh, "killall -9 memcached", check=False)

	# stop master
	tc.log_print("Stopping master...")
	tc.remote_exec(master_ssh, "killall -9 mutilate", check=False)

def run_exp(sc, ld, lstat):
	while True:
		if client_only:
			ssrv = None
		else:
			# start server
			tc.log_print("Starting server...")
			server_cmd = None
			if sc == -1:
				server_cmd = tc.get_cpuset_core(threads) + " " + test_dir + "/memcached/memcached -m 1024 -c 65536 -b 4096 -t " + str(threads)
			elif sc == -2:
				server_cmd = "limit core 0; sudo " + tc.get_cpuset_core(threads) + " " + \
												test_dir + "/memcached_linox/memcached -u oscar -m 1024 -c 65536 -b 4096 -t " + str(threads)
			elif sc == -3:
				server_cmd = "limit core 0; sudo " + tc.get_cpuset_core(threads) + " " + test_dir + "/memcached-A/memcached -u oscar -m 1024 -c 65536 -b 4096 -t 1 " + \
																	"--minNumCores 2 --maxNumCores " + str(threads - 1)
			else:
				server_cmd = test_dir + "/mem/memcached -e -m 1024 -c 65536 -b 4096 -t " + str(threads) + " -q " + str(sc) + (" -j 1 " if dump else "")

			if lstat:
				server_cmd = "sudo lockstat -A -P -s16 -n16777216 " + server_cmd + " -u " + tc.get_username()

			tc.log_print(server_cmd)
			ssrv = tc.remote_exec(server_ssh, server_cmd, blocking=False)

		# pre-load server
		time.sleep(1)
		tc.log_print("Preloading server...")
		preload_cmd = test_dir + "/mutilate/mutilate --loadonly -s localhost"
		tc.log_print(preload_cmd)
		tc.remote_exec(server_ssh, preload_cmd, blocking=True)

		time.sleep(1)
		# start clients
		tc.log_print("Starting clients...")
		client_cmd = tc.get_cpuset_core(client_threads) + " " + test_dir + "/mutilate/mutilate -A -T " + str(client_threads)
		tc.log_print(client_cmd)
		sclt = tc.remote_exec(clients_ssh, client_cmd, blocking=False)

		time.sleep(1)
		# start master
		tc.log_print("Starting master...")
		master_cmd = test_dir + "/mutilate/mutilate --noload -K fb_key -V fb_value -i fb_ia -u 0.03 -Q 1000 " + \
		                                " -T " + str(client_threads) + \
									    " -C 1 " + \
										" -c " + str(conn_per_thread) + \
										" -w " + str(warmup) + \
										" -t " + str(duration) + \
										" -s " + server[0] + " " + get_client_str(clients) + \
										" -q " + str(ld) + \
										" --save " + test_dir + "/" + sample_filename
		tc.log_print(master_cmd)
		sp = tc.remote_exec(master_ssh, master_cmd, blocking=False)
		p = sp[0]

		success = False
		cur = 0
		while True:
			# either failed or timeout
			# we use failure detection to save time for long durations
			if False \
				or not tc.scan_stderr(ssrv, exclude=[".*warn.*", ".*DEBUG.*", ".*"]) \
				or not tc.scan_stderr(sclt) \
				or cur >= int(warmup + duration) * 3 \
				or not tc.scan_stderr(sp, exclude=[".*mutex.hpp.*"]) \
					:
				break
			
			if p.poll() != None:
				success = True
				break

			time.sleep(1)
			cur = cur + 1

		stop_all()

		tc.log_print("Cooling down...")
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
			return output, stdout, stderr

def keep_results(ld, output, sout, serr):
	f = open(tc.get_odir() + "/l" + ld +".txt", "w")
	f.write(output)
	f.close()

	scpcmd = "scp " + master_ssh[0] + ":" + test_dir + "/" + sample_filename + " " + tc.get_odir() + "/l" + ld + ".sample"
	tc.log_print(scpcmd)
	sp.check_call(scpcmd, shell=True)

	if lockstat and len(sout) > 0:
		f = open(tc.get_odir() + "/l" + ld  + (".lstat" if lockstat else ".truss"), "w")
		f.write(sout)
		f.close()
	
	if dump and len (sout) > 0:
		f = open(tc.get_odir() + "/l" + ld + ".kstat", "w")
		f.write(sout)
		f.close()

def main():
	global hostfile
	global server
	global clients
	global dump
	global lockstat
	global client_only
	global master

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

	tc.init("~/results.d/mem/" + str(threads) + "+" + str(len(clients)) + "x" + str(client_threads) + "x" + str(conn_per_thread))

	tc.log_print("Configuration:\n" + \
		  "Hostfile: " + ("None" if hostfile == None else hostfile) + "\n" \
		  "Lockstat: " + str(lockstat) + "\n" \
		  "KQ dump: " + str(dump) + "\n" \
		  "Client only: " + str(client_only) + "\n")

	if hostfile != None:
		hosts = tc.parse_hostfile(hostfile)
		server = tc.process_hostnames(server, hosts)
		clients = tc.process_hostnames(clients, hosts)
		master = tc.process_hostnames(master, hosts)

	stop_all()

	for i in range(0, len(sched), 2):
		esched = sched[i+1]
		ename = sched[i]

		tc.begin(ename)

		tc.log_print("============ Sched: " + str(ename) + " Flag: " + format(esched, '#04x') + " Load: MAX ============")

		output, sout, serr = run_exp(esched, 0, lockstat)

		keep_results("max", output, sout, serr)

		if lockstat:
			# lockstat only supports max throughput
			continue

		tc.log_print(output)
		
		step_mul = 100
		last_load = 0
		cur_load = init_step
		while True:
			tc.log_print("============ Sched: " + ename +  " Flag: " + format(esched, '#04x') + " Load: " + str(cur_load) + " ============")
			output, sout, serr = run_exp(esched, cur_load, lockstat)

			parse = mp.parse_mut_output(output)

			tc.log_print(output)
			
			keep_results(str(int(parse.qps)), output, sout, serr)

			pct = int((parse.qps - last_load) / init_step * 100)
			tc.log_print("last_load: " + str(last_load) + " this_load: " + str(parse.qps) + " inc_pct: " + str(pct) + "%")

			if pct <= term_pct:
				tc.log_print("inc_pct less than TERM_PCT " + str(term_pct) + "%. Done.")
				break

			if pct <= inc_pct:
				step_mul += step_inc_pct
				tc.log_print("inc_pct less than INC_PCT " + str(inc_pct) + "%. Increasing step multiplier to " + str(step_mul) + "%")

			last_load = parse.qps
			cur_load += int(init_step * step_mul / 100)
			tc.log_print("")

		tc.end()
	
	stop_all()

main()
