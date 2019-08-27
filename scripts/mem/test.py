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

def make_sched_flag(sched, args):
	return (sched & 0xFF) | (args & 0xFF) << 8

sched = [
	"vanilla", -1,
	"queue0", make_sched_flag(1, 0),
	"queue1", make_sched_flag(1, 1),
	"queue2", make_sched_flag(1, 2),
	"cpu0", make_sched_flag(2, 0),
	"cpu1", make_sched_flag(2, 1),
	"cpu2", make_sched_flag(2, 2),
	"best2", make_sched_flag(4, 2),
	"rand", make_sched_flag(0, 0),
]

load = [50, 80, 100]
#10, 30, 50, 60, 70, 80, 90, 100, 125, 150

master = ["localhost"]
server = ["skylake1"]
clients = ["skylake2", "skylake3", "skylake4", "skylake5", "skylake6", "skylake7", "skylake8"]

threads = 12
client_threads = 48
warmup = 5
duration = 10
conn_per_thread = 8
hostfile = None

def get_username():
    return pwd.getpwuid( os.getuid() )[0]

def get_cpu_str(threads):
	ret = "cpuset -l 0-23" # + str(threads - 1)
	return ret

def remote_action(srv, cmd, blocking=True):
	sub = []
	# start clients
	for client in srv:
		p = sp.Popen(["ssh " + client + " " + cmd], shell=True, stdout=sp.PIPE, stderr=sp.PIPE)
		sub.append(p)
	
	if blocking:
		for p in sub:
			p.wait()

	return sub

def get_client_str(cl):
	ret = ""
	for client in cl:
		ret += " -a " + client
	return ret

def stop_all():
	# stop clients
	print("Stopping clients...")
	remote_action(clients, "killall mutilate")

	# stop server
	print("Stopping server...")
	remote_action(server, "killall memcached")

	# stop master
	print("Stopping master...")
	remote_action(master, "killall mutilate")


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
			print("Error detected: idx - " + str(i) + " " + line)
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
			print("Parsed: hostname \"" + spl[0] + "\" -> \"" + spl[1] + "\"")
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
		print("Starting server...")
		server_cmd = None
		if sc == -1:
			server_cmd = get_cpu_str(threads) + " " + test_dir + "/memcached/memcached -m 1024 -c 65536 -b 4096 -t " + str(threads)
		else:
			server_cmd = test_dir + "/mem/memcached -e -m 1024 -c 65536 -b 4096 -t " + str(threads) + " -q " + str(sc)
		
		if lstat:
			server_cmd = "sudo lockstat -A -P -s4 -n16777216 " + server_cmd + " -u " + get_username()

		print(server_cmd)
		ssrv = remote_action(server, server_cmd, blocking=False)

		# start clients
		print("Starting clients...")
		client_cmd = get_cpu_str(client_threads) + " " + test_dir + "/mutilate/mutilate -A -T " + str(client_threads)
		print(client_cmd)
		sclt = remote_action(clients, client_cmd, blocking=False)

		time.sleep(1)
		# start master
		print("Starting master...")
		master_cmd = test_dir + "/mutilate/mutilate -K fb_key -V fb_value -i fb_ia -u 0.03 " + \
										" -c " + str(conn_per_thread) + \
										" -w " + str(warmup) + \
										" -t " + str(duration) + \
										" -s " + server[0] + " " + get_client_str(clients) + \
										" -q " + str(ld)
		print(master_cmd)
		sp = remote_action(master, master_cmd, blocking=False)
		p = sp[0]

		success = False
		cur = 0
		while True:
			# either failed or timeout
			# we use failure detection to save time for long durations
			if not scan_stderr(ssrv, "warning" if lstat else None) \
				or not scan_stderr(sp, "mutex.hpp") \
				or not scan_stderr(sclt) \
				or cur >= int(warmup + duration) * 2 :
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

def main():
	global hostfile
	global server
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
			

	print("Configuration:\n" + \
		  "Hostfile: " + ("None" if hostfile == None else hostfile) + "\n" \
		  "Lockstat: " + str(lockstat))

	if hostfile != None:
		hosts = parse_host_file(hostfile)
		server = process_hostnames(server, hosts)
		clients = process_hostnames(clients, hosts)

	dirname = "results.d/" + str(len(clients)) + "x" + str(threads) + "x" + str(conn_per_thread) + "_" + \
							datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S') + \
							("_lstat" if lockstat else "")
	sp.check_call(["mkdir -p " + dirname], shell=True)

	print("Results dir: " + dirname + "\n")

	stop_all()

	for i in range(0, len(sched), 2):
		esched = sched[i+1]
		ename = sched[i]
		eachdir = dirname + "/" + ename
		sp.call(["mkdir -p " + eachdir], shell=True)

		print("============ Sched: " + str(ename) + " Flag: " + str(esched) + " Load: MAX ============")
		output = run_exp(esched, 0, lockstat)

		f = open(eachdir + "/lmax.txt", "w")
		f.write(output)
		f.close()
		print("")

		# lockstat only supports max throughput
		if lockstat:
			continue
		
		print(output)
		mdat = memparse.parse(output)
		max_load = int(mdat.qps)
		
		if max_load != None:
			print("Max load: " + str(max_load))
		else:
			print("Failed to obtain the maximum load")
			exit(1)
	
		for eload in load:
			print("============ Sched: " + ename +  " Flag: " + str(esched) + " Load: " + str(eload) + "% ============")
			real_load = int(max_load * eload / 100)
			output = run_exp(esched, real_load, lockstat)

			print(output)
			f = open(eachdir + "/l" + str(eload) + ".txt", "w")
			f.write(output)
			f.close()
			print("")

	stop_all()

main()
