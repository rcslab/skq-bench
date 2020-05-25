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

# paths
test_dir = "/tmp/tests.d/"
file_dir = os.path.dirname(os.path.realpath(__file__))
root_dir = file_dir + "/../../"
sample_filename = "sample.txt"

sched = [
	"cpu0", tc.make_sched_flag(tc.SCHED_CPU, 0),
	#"multiple_skq", -1,
	#"vanilla_single", -2,
]

master = ["skylake2"]
server = ["skylake1"]
clients = ["skylake3", "skylake4", "skylake5", "skylake6", "skylake7", "skylake8", "sandybridge1", "sandybridge2", "sandybridge3", "sandybridge4"]

threads = [12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]

client_threads = 12
warmup = 3
duration = 3
cooldown = 0
conn_per_thread = 1

server_delay = False
conn_delay = False


hostfile = None
dump = False
lockstat = False
client_only = False

def stop_all():
	# stop clients
	tc.log_print("Stopping clients...")
	tc.remote_exec(clients, "killall -9 dismember", check=False)

	if not client_only:
		# stop server
		tc.log_print("Stopping server...")
		tc.remote_exec(server, "killall -9 ppd", check=False)

	# stop master
	tc.log_print("Stopping master...")
	tc.remote_exec(master, "killall -9 dismember", check=False)

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
			server_cmd = test_dir + "/pingpong/build/ppd -a -t " + str(ld) + " -M 0 "

			if server_delay:
				server_cmd += " -D "

			if conn_delay:
				server_cmd += " -c "

			if lstat:
				server_cmd = "sudo lockstat -A -P -s4 -n16777216 " + server_cmd
			
			if sc == -2:
				server_cmd = server_cmd + " -m -1 "
			elif sc != -1: 
				server_cmd = server_cmd + " -m " + str(sc)
				if dump:
					server_cmd += " -d 1 "

			tc.log_print(server_cmd)

			ssrv = tc.remote_exec(server, server_cmd, blocking=False)

		# start clients
		tc.log_print("Starting clients...")
		client_cmd = tc.get_cpuset_core(client_threads) + " " + test_dir + "/pingpong/build/dismember -A"
		tc.log_print(client_cmd)
		sclt = tc.remote_exec(clients, client_cmd, blocking=False)

		time.sleep(1)
		# start master
		tc.log_print("Starting master...")
		master_cmd = tc.get_cpuset_core(client_threads) + " " + test_dir + "/pingpong/build/dismember " + \
			                  get_client_str(clients) + \
							  " -s " + server[0] + \
							  " -q 0" + \
							  " -c " + str(conn_per_thread) + \
							  " -o " + test_dir + "/" + sample_filename + \
							  " -t " + str(client_threads) + \
							  " -w " + str(duration) + \
							  " -W " + str(warmup) + \
                              " -T " + str(client_threads) + \
							  " -i exponential " + \
							  " -C 12 " + \
							  " -Q 1000 " + \
							  " -l 0 " + \
                              " -OGEN=fixed:5 "


		tc.log_print(master_cmd)
		sp = tc.remote_exec(master, master_cmd, blocking=False)
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

def keep_results(ethread, output, sout, serr):
	scpcmd = "scp " + master[0] + ":" + test_dir + "/" + sample_filename + " " + tc.get_odir() + "/sample.txt"
	tc.log_print(scpcmd)
	sp.check_call(scpcmd, shell=True)

	# parse the output file for percentile and average load
	qps, lat = mp.parse_mut_sample(tc.get_odir() + "/sample.txt")
	avg_qps = np.mean(qps)
	
	output = build_memparse(lat, qps)
	f = open(tc.get_odir() + "/t" + str(ethread) + ".txt", "w")
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
	global clients
	global dump
	global lockstat
	global client_only
	global server_delay

	options = getopt.getopt(sys.argv[1:], 'h:sldcDp')[0]
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
		elif opt in ('-D'):
			server_delay=True

	tc.init("scale_" + str(len(clients)) + "x" + str(client_threads) + "x" + str(conn_per_thread))

	tc.log_print("Configuration:\n" + \
		  "Hostfile: " + ("None" if hostfile == None else hostfile) + "\n" \
		  "Lockstat: " + str(lockstat) + "\n" \
		  "KQ dump: " + str(dump) + "\n" \
		  "Client only: " + str(client_only) + "\n" + \
		  "Server delay: " + str(server_delay) + "\n" + \
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

		# output, sout, serr = run_exp(esched, 12, lockstat)
		# keep_results(99, output, sout, serr)
		# stop_all()

		tc.begin(ename)
		for j in range(0, len(threads)):
			ethread = threads[j]
			
			tc.log_print("============ Sched: " + str(ename) + " Thread: " + str(ethread) + " ============")

			output, sout, serr = run_exp(esched, ethread, lockstat)

			keep_results(ethread, output, sout, serr)

			tc.log_print("")
		
		tc.end()

	stop_all()

main()
