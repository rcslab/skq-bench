import subprocess as sp
import time
import select
import os, datetime
import sys
import getopt

# paths
test_dir = "/tmp/tests.d"
file_dir = os.path.dirname(os.path.realpath(__file__))
root_dir = file_dir + "/../../"

load = [100, 110, 125, 150]
sched = [6]

server = ["skylake1"]
clients = ["skylake2", "skylake3", "skylake4", "skylake5", "skylake6", "skylake7", "skylake8"]

threads = 12
warmup = 5
duration = 10
conn_per_thread = 16

vanilla = False
hostfile = None


def get_cpu_str(threads):
	ret = "cpuset -l 0-23" # + str(threads - 1)
	return ret

def remote_action(srv, cmd, blocking=True):
	sub = []
	# start clients
	for client in srv:
		if not blocking:
			p = sp.Popen(["ssh -p77 " + client + " " + cmd + " & "], shell=True, stdout=sp.PIPE, stderr=sp.STDOUT)
			sub.append(p)
		else:
			sp.call(["ssh -p77 " + client + " " + cmd], shell=True)
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
	sp.call(["killall mutilate"], shell=True)


def scan_stderr(cp):
	sels = []
	for i in range(len(cp)):
		sel = select.poll()
		sel.register(cp[i].stdout, select.POLLIN)
		sels.append(sel)
	
	for i in range(len(sels)):
		while True:
			events = sels[i].poll(1)

			if len(events) is 0:
				break

			line = cp[i].stdout.readline().decode("utf-8")
			if line.find('ERROR') != -1:
				print("Error detected: idx - " + str(i) + " " + line)
				return True
	return False

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

def run_exp(sc, ld):
	success = False
	while not success:
		stop_all()

		# start clients
		print("Starting clients...")
		client_cmd = get_cpu_str(threads) + " " + test_dir + "/mutilate/mutilate -A -T " + str(threads)
		print(client_cmd)
		sclient = remote_action(clients, client_cmd, blocking=False)

		# start server
		print("Starting server...")
		server_cmd = get_cpu_str(threads) + " " + ( test_dir + "/memcached/memcached" if vanilla else test_dir + "/mem/memcached") + \
					" -m 1024 -c 65536 -b 4096 -t " + str(threads) + ("" if vanilla else " -q " + str(sc))
		print(server_cmd)
		remote_action(server, server_cmd, blocking=False)

		time.sleep(1)
		# start master
		print("Starting master...")
		master_cmd = root_dir + "/mutilate/mutilate -K fb_key -V fb_value -i fb_ia " + \
										" -c " + str(conn_per_thread) + \
										" -w " + str(warmup) + \
										" -t " + str(duration) + \
										" -s " + server[0] + " " + get_client_str(clients) + \
										" -q " + str(ld)
		print(master_cmd)
		p = sp.Popen(master_cmd, shell=True, stdout=sp.PIPE, stderr=sp.STDOUT)

		cur = 0
		while True:
			if p.poll() is not None:
				success = True
				break
			else:
				# either failed or timeout
				# we use failure detection to save time for long durations
				if scan_stderr(sclient) or cur >= int((warmup + duration) * 2):
					break
			time.sleep(1)
			cur = cur + 1
				

		if success:
			output = p.communicate()[0].decode("utf-8")
			return output

def main():
	global hostfile
	global vanilla
	global server
	global clients

	options = getopt.getopt(sys.argv[1:], 'h:v')[0]

	for opt, arg in options:
		if opt in ('-h'):
			hostfile = arg
		elif opt in ('-v'):
			vanilla = True

	print("Configuration:\n" + "Vanilla: " + str(vanilla) + "\n" + "Hostfile: " + ("None" if hostfile == None else hostfile) + "\n")

	if hostfile != None:
		hosts = parse_host_file(hostfile)
		server = process_hostnames(server, hosts)
		clients = process_hostnames(clients, hosts)

	dirname = str(len(clients)) + "*" + str(threads) + "*" + str(conn_per_thread) + "_" + datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
	sp.call(["mkdir -p results.d/" + dirname], shell=True)

	print("")

	for esched in sched:
		if vanilla:
			esched = -1
		print("============ Sched " + str(esched) + " Load: MAX ============")
		print("- Determining the maximum load...")
		output = run_exp(esched, 0)
		print(output)

		f = open("results.d/" + dirname + "/s" + str(esched) + "_lmax.txt", "w")
		f.write(output)
		f.close()
		print("")
		
		max_load = None
		for line in output.splitlines():
			if line.find("Total QPS") != -1:
				spl = line.split(" ")
				if len(spl) == 7:
					max_load = int(float(spl[3]))
					break
		
		if max_load != None:
			print("Max load: " + str(max_load))
		else:
			print("Failed to obtain the maximum load")
			exit(1)
	
		for eload in load:
			print("============ Sched " + str(esched) + " Load: " + str(eload) + "% ============")
			real_load = int(max_load * eload / 100)
			output = run_exp(esched, real_load)

			print(output)
			f = open("results.d/" + dirname + "/s" + str(esched) + "_l" + str(eload) + ".txt", "w")
			f.write(output)
			f.close()
			print("")

		if vanilla:
			break

	stop_all()

main()
