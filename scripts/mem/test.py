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
import memparse

# paths
test_dir = "/tmp/tests.d"
file_dir = os.path.dirname(os.path.realpath(__file__))
root_dir = file_dir + "/../../"

sample_filename = "sample.txt"

log_filename = "log.txt"
log_file = None

def log_print(info):
	print(info)
	if log_file != None:
		log_file.write(info + "\n")
		log_file.flush()

def make_sched_flag(sched, args, feat = 0, fargs = 0):
	return (sched & 0xFF) | (args & 0xFF) << 8 | (feat & 0xFF) << 16 | (fargs & 0xFF) << 24


SCHED_VANILLA=-1
SCHED_ARACHNE=-2
SCHED_VANILLA_LINOX=-3

SCHED_QUEUE = 1
SCHED_CPU = 2
SCHED_BEST = 4

SCHED_FEAT_WS = 1

sched = [
	"vanilla", -1,
	"best2", make_sched_flag(SCHED_BEST, 2),
	"best2_ws", make_sched_flag(SCHED_BEST, 2, feat=SCHED_FEAT_WS, fargs=1),
	"cpu0", make_sched_flag(SCHED_CPU, 0),
	"queue0", make_sched_flag(SCHED_QUEUE, 0),
	"queue1", make_sched_flag(SCHED_QUEUE, 1),
    "queue2", make_sched_flag(SCHED_QUEUE, 2),
	"q0_ws", make_sched_flag(SCHED_QUEUE, 0, feat=SCHED_FEAT_WS, fargs=1),
	"q1_ws", make_sched_flag(SCHED_QUEUE, 1, feat=SCHED_FEAT_WS, fargs=1),
	"q2_ws", make_sched_flag(SCHED_QUEUE, 2, feat=SCHED_FEAT_WS, fargs=1),
	"cpu0_ws", make_sched_flag(SCHED_CPU, 0, feat=SCHED_FEAT_WS, fargs=1),
    "cpu1_ws", make_sched_flag(SCHED_CPU, 1, feat=SCHED_FEAT_WS, fargs=1),
    "cpu2_ws", make_sched_flag(SCHED_CPU, 2, feat=SCHED_FEAT_WS, fargs=1),
	"cpu1", make_sched_flag(SCHED_CPU, 1),
	"cpu2", make_sched_flag(SCHED_CPU, 2),
    #"rand", make_sched_flag(0, 0),
	#"arachne", SCHED_ARACHNE,
	#"linox", SCHED_VANILLA_LINOX
]


step_inc_pct = 100
init_step = 100000

term_pct = 5
inc_pct = 50

master = ["skylake2"]
server = ["skylake1"]
clients = ["skylake3", "skylake4", "skylake5", "skylake6", "skylake7", "skylake8"]

threads = 12
client_threads = 12
warmup = 5
duration = 10
cooldown = 0
conn_per_thread = 8
hostfile = None
dump = False
lockstat = False
truss = False
client_only = False

def get_username():
    return pwd.getpwuid( os.getuid() )[0]

def get_cpu_str():
	ret = "cpuset -l 0-23 "
	return ret

def remote_action(srv, cmd, blocking=True, check=True, verbose=False):
	sub = []
	# start clients
	for client in srv:
		p = sp.Popen(["ssh " + client + " \"" + cmd +"\""], shell=True, stdout=sp.PIPE, stderr=sp.PIPE)
		sub.append(p)
	
	if blocking:
		for p in sub:
			p.wait()
			if check and p.returncode != 0:
				raise Exception("Command failed " + cmd)

	return sub

def get_client_str(cl):
	ret = ""
	for client in cl:
		ret += " -a " + client
	return ret

def stop_all():
	# stop clients
	log_print("Stopping clients...")
	remote_action(clients, "killall -9 mutilate", check=False)

	if not client_only:
		# stop server
		log_print("Stopping server...")
		remote_action(server, "killall -9 memcached; sudo killall -9 lockstat", check=False)

	# stop master
	log_print("Stopping master...")
	remote_action(master, "killall -9 mutilate", check=False)


def scan_stderr(cp, exclude = None):
	if cp == None:
		return True
		
	sels = []
	for i in range(len(cp)):
		sel = select.poll()
		sel.register(cp[i].stderr, select.POLLIN)
		sels.append(sel)
	for i in range(len(sels)):
		while True:
			events = sels[i].poll(1)
			
			if len(events) is 0:
				break

			line = cp[i].stderr.readline()
			line = line.decode(sys.getfilesystemencoding())
			line = line.strip()
			if len(line) == 0:
				break

			if exclude != None:
				for exc in exclude:
					if (exc != None) and (re.match(exc, line) != None):
						return True
			
			log_print("Error detected: idx - " + str(i) + " " + line)
			return False
			
	return True

def parse_host_file(fp):
	ret = {}
	fh = open(fp, "r")
	content = fh.readlines()
	fh.close()
	content = [x.strip() for x in content]
	for line in content:
		spl = line.split(" ")
		if len(spl) >= 2:
			ret[spl[0]] = spl[1]
			log_print("Parsed: hostname \"" + spl[0] + "\" -> \"" + spl[1] + "\"")
	return ret

def process_hostnames(names, hosts):
	ret = []
	for line in names:
		if hosts[line] != None:
			ret.append(hosts[line])
		else:
			ret.append(line)
	return ret

def run_exp(sc, ld, lstat):
	while True:
		if client_only:
			ssrv = None
		else:
			# start server
			log_print("Starting server...")
			server_cmd = None
			if sc == SCHED_VANILLA:
				server_cmd = get_cpu_str() + " " + test_dir + "/memcached/memcached -m 1024 -c 65536 -b 4096 -t " + str(threads)
			elif sc == SCHED_VANILLA_LINOX:
				server_cmd = "limit core 0; export EVENT_NOEPOLL=1; " + get_cpu_str() + " " + \
												test_dir + "/memcached_linox/memcached -m 1024 -c 65536 -b 4096 -t " + str(threads)
			elif sc == SCHED_ARACHNE:
				server_cmd = "limit core 0; " + get_cpu_str() + " " + test_dir + "/memcached-A/memcached -m 1024 -c 65536 -b 4096 -t 1 " + \
																	"--minNumCores 2 --maxNumCores " + str(threads - 1)
			else:
				server_cmd = test_dir + "/mem/memcached -e -m 1024 -c 65536 -b 4096 -t " + str(threads) + " -q " + str(sc) + (" -j 1 " if dump else "")

			if lstat:
				server_cmd = "sudo lockstat -A -P -s4 -n16777216 " + server_cmd + " -u " + get_username()

			log_print(server_cmd)
			ssrv = remote_action(server, server_cmd, blocking=False)

		# start clients
		log_print("Starting clients...")
		client_cmd = get_cpu_str() + " " + test_dir + "/mutilate/mutilate -A -T " + str(client_threads)
		log_print(client_cmd)
		sclt = remote_action(clients, client_cmd, blocking=False)

		time.sleep(1)
		# start master
		log_print("Starting master...")
		master_cmd = get_cpu_str() + test_dir + "/mutilate/mutilate -K fb_key -V fb_value -i fb_ia -u 0.03 -Q 1000 -T " + str(client_threads) + \
									    " -C 1 " + \
										" -c " + str(conn_per_thread) + \
										" -w " + str(warmup) + \
										" -t " + str(duration) + \
										" -s " + server[0] + " " + get_client_str(clients) + \
										" -q " + str(ld) + \
										" --save " + test_dir + "/" + sample_filename
		log_print(master_cmd)
		sp = remote_action(master, master_cmd, blocking=False)
		p = sp[0]

		success = False
		cur = 0
		while True:
			# either failed or timeout
			# we use failure detection to save time for long durations
			if False \
				or not scan_stderr(ssrv, exclude=[".*warn.*", ".*DEBUG.*", (".*" if truss else None)]) \
				or not scan_stderr(sclt) \
				or cur >= int(warmup + duration) * 2 \
				or not scan_stderr(sp, exclude=[".*mutex.hpp.*"]) \
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

def keep_results(eachdir, ld, output, sout, serr):
	# keep generic results
	f = open(eachdir + "/l" + ld +".txt", "w")
	f.write(output)
	f.close()

	scpcmd = "scp " + master[0] + ":" + test_dir + "/" + sample_filename + " " + eachdir + "/l" + ld + ".sample"
	log_print(scpcmd)
	sp.check_call(scpcmd, shell=True)

	if (lockstat or truss) and len(serr) > 0:
		f = open(eachdir + "/l" + ld  + (".lstat" if lockstat else ".truss"), "w")
		f.write(serr)
		f.close()
	
	if dump and len (sout) > 0:
		f = open(eachdir + "/l" + ld + ".kstat", "w")
		f.write(sout)
		f.close()

def main():
	global hostfile
	global server
	global log_file
	global log_filename
	global clients
	global dump
	global lockstat
	global truss
	global client_only

	options = getopt.getopt(sys.argv[1:], 'h:sldtc')[0]
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
		elif opt in ('-t'):
			truss=True
		elif opt in ('-c'):
			client_only=True

	if truss:
		print("fuck truss piece of complete scheisse. Just smash your server instead.")
		exit(-65536)

	dirname = "results.d/" + datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S') + \
							"_" + str(threads) + "+" + str(len(clients)) + "x" + str(client_threads) + "x" + str(conn_per_thread)

	sp.check_call(["mkdir -p " + dirname], shell=True)

	log_file = open(dirname + "/" + log_filename, "w")
	log_print("Results dir: " + dirname + "\n")

	log_print("Configuration:\n" + \
		  "Hostfile: " + ("None" if hostfile == None else hostfile) + "\n" \
		  "Lockstat: " + str(lockstat) + "\n" \
		  "KQ dump: " + str(dump) + "\n" \
		  "Truss: " + str(truss) + "\n" \
		  "Client only: " + str(client_only) + "\n")

	if hostfile != None:
		hosts = parse_host_file(hostfile)
		server = process_hostnames(server, hosts)
		clients = process_hostnames(clients, hosts)
		master = process_hostnames(master, hosts)

	stop_all()

	for i in range(0, len(sched), 2):
		esched = sched[i+1]
		ename = sched[i]
		eachdir = dirname + "/" + ename
		sp.check_call(["mkdir -p " + eachdir], shell=True)

		log_print("============ Sched: " + str(ename) + " Flag: " + format(esched, '#04x') + " Load: MAX ============")

		output, sout, serr = run_exp(esched, 0, lockstat)

		keep_results(eachdir, "max", output, sout, serr)

		if lockstat:
			# lockstat only supports max throughput
			continue

		log_print(output)
		
		step_mul = 100
		last_load = 0
		cur_load = init_step
		while True:
			log_print("============ Sched: " + ename +  " Flag: " + format(esched, '#04x') + " Load: " + str(cur_load) + " ============")
			output, sout, serr = run_exp(esched, cur_load, lockstat)

			parse = memparse.parse(output)

			log_print(output)
			
			keep_results(eachdir, str(int(parse.qps)), output, sout, serr)

			pct = int((parse.qps - last_load) / init_step * 100)
			log_print("last_load: " + str(last_load) + " this_load: " + str(parse.qps) + " inc_pct: " + str(pct) + "%")

			if pct <= term_pct:
				log_print("inc_pct less than TERM_PCT " + str(term_pct) + "%. Done.")
				break

			if pct <= inc_pct:
				step_mul += step_inc_pct
				log_print("inc_pct less than INC_PCT " + str(inc_pct) + "%. Increasing step multiplier to " + str(step_mul) + "%")

			last_load = parse.qps
			cur_load += int(init_step * step_mul / 100)
			log_print("")
			
	
	stop_all()
	log_file.close()

main()
