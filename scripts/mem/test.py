#!/usr/bin/env python3.6
import subprocess as sp
import time
import select
import os
import datetime
import pwd
import sys
import getopt
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

def make_sched_flag(sched, args):
	return (sched & 0xFF) | (args & 0xFF) << 8

SCHED_VANILLA=-1
SCHED_ARACHNE=-2
SCHED_VANILLA_LINOX=-3

sched = [
	"vanilla", -1,
    "queue0", make_sched_flag(1, 0),
    "queue1", make_sched_flag(1, 1),
    "queue2", make_sched_flag(1, 2),
    "cpu0", make_sched_flag(2, 0),
    "cpu1", make_sched_flag(2, 1),
    "cpu2", make_sched_flag(2, 2),
    "best2", make_sched_flag(4, 2),
    #"rand", make_sched_flag(0, 0),
	#"arachne", SCHED_ARACHNE,
	#"linox", SCHED_VANILLA_LINOX
]
step_inc_pct = 100
init_step = 100000

term_pct = 5
inc_pct = 50

master = ["localhost"]
server = ["skylake1"]
clients = ["skylake2", "skylake3", "skylake4", "skylake5", "skylake6", "skylake7", "skylake8"]

threads = 12
client_threads = 12
warmup = 3
duration = 5
conn_per_thread = 12
hostfile = None

def get_username():
    return pwd.getpwuid( os.getuid() )[0]

def get_cpu_str(threads):
	ret = "cpuset -l 0-23" # + str(threads - 1)
	return ret

def remote_action(srv, cmd, blocking=True, check=True):
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
	remote_action(clients, "killall mutilate", check=False)

	# stop server
	log_print("Stopping server...")
	remote_action(server, "killall memcached", check=False)

	# stop master
	log_print("Stopping master...")
	remote_action(master, "killall mutilate", check=False)


def scan_stderr(cp, exclude = None):
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

			line = cp[i].stderr.readline().decode(sys.getfilesystemencoding())
			if exclude != None:
				if line.find(exclude) != -1:
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
		# start server
		log_print("Starting server...")
		server_cmd = None
		if sc == SCHED_VANILLA:
			server_cmd = get_cpu_str(threads) + " " + test_dir + "/memcached/memcached -m 1024 -c 65536 -b 4096 -t " + str(threads)
		elif sc == SCHED_VANILLA_LINOX:
			server_cmd = "limit core 0; export EVENT_NOEPOLL=1; " + get_cpu_str(threads) + " " + \
											test_dir + "/memcached_linox/memcached -m 1024 -c 65536 -b 4096 -t " + str(threads)
		elif sc == SCHED_ARACHNE:
			server_cmd = "limit core 0; " + get_cpu_str(threads) + " " + test_dir + "/memcached-A/memcached -m 1024 -c 65536 -b 4096 -t 1 " + \
																 "--minNumCores 2 --maxNumCores " + str(threads - 1)
		else:
			server_cmd = test_dir + "/mem/memcached -e -m 1024 -c 65536 -b 4096 -t " + str(threads) + " -q " + str(sc)

		if lstat:
			server_cmd = "sudo lockstat -A -P -s4 -n16777216 " + server_cmd + " -u " + get_username()

		log_print(server_cmd)
		ssrv = remote_action(server, server_cmd, blocking=False)
 
		# start clients
		log_print("Starting clients...")
		client_cmd = get_cpu_str(client_threads) + " " + test_dir + "/mutilate/mutilate -A -T " + str(client_threads)
		log_print(client_cmd)
		sclt = remote_action(clients, client_cmd, blocking=False)

		time.sleep(1)
		# start master
		log_print("Starting master...")
		master_cmd = test_dir + "/mutilate/mutilate -K fb_key -V fb_value -i fb_ia -u 0.03 " + \
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
			#or not scan_stderr(ssrv, "DEBUG" if lstat else None) \
			if False \
				or not scan_stderr(sp, "mutex.hpp") \
				or not scan_stderr(sclt) \
				or cur >= int(warmup + duration) * 3 \
					:
				break

			if p.poll() != None:
				success = True
				break

			time.sleep(1)
			cur = cur + 1

		stop_all()
			
		if success:
			if lstat:
				return ssrv[0].communicate()[1].decode(sys.getfilesystemencoding())
			else:
				return p.communicate()[0].decode(sys.getfilesystemencoding())

def keep_results(eachdir, ld, output):
	f = open(eachdir + "/l" + ld +".txt", "w")
	f.write(output)
	f.close()

	#bzcmd = "bzip2 " + test_dir + "/" + sample_filename
	#log_print(bzcmd)
	#remote_action(master, bzcmd)

	scpcmd = "scp " + master[0] + ":" + test_dir + "/" + sample_filename + " " + eachdir + "/l" + ld + "_sample.txt"
	log_print(scpcmd)
	sp.check_call(scpcmd, shell=True)

def main():
	global hostfile
	global server
	global log_file
	global log_filename
	global clients

	options = getopt.getopt(sys.argv[1:], 'h:sl')[0]
	lockstat = False
	for opt, arg in options:
		if opt in ('-h'):
			hostfile = arg
		elif opt in ('-s'):
			stop_all()
			return
		elif opt in ('-l'):
			lockstat=True
			
	dirname = "results.d/" + datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S') + \
							"_" + str(threads) + "+" + str(len(clients)) + "x" + str(client_threads) + "x" + str(conn_per_thread) + \
							("_lstat" if lockstat else "")
	sp.check_call(["mkdir -p " + dirname], shell=True)

	log_file = open(dirname + "/" + log_filename, "w")
	log_print("Results dir: " + dirname + "\n")

	log_print("Configuration:\n" + \
		  "Hostfile: " + ("None" if hostfile == None else hostfile) + "\n" \
		  "Lockstat: " + str(lockstat))

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

		log_print("============ Sched: " + str(ename) + " Flag: " + str(esched) + " Load: MAX ============")

		output = run_exp(esched, 0, lockstat)

		keep_results(eachdir, "max", output)

		if lockstat:
			# lockstat only supports max throughput
			continue

		log_print(output)
		
		step_mul = 100
		last_load = 0
		cur_load = init_step
		while True:
			log_print("============ Sched: " + ename +  " Flag: " + str(esched) + " Load: " + str(cur_load) + " ============")
			output = run_exp(esched, cur_load, lockstat)

			parse = memparse.parse(output)

			keep_results(eachdir, str(int(parse.qps)), output)

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
