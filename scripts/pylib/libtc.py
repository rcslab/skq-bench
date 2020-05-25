#!/usr/bin/env python3.6
import subprocess as sp
import time
import select
import os
import pwd
import sys
import datetime
import re

tc_logfile = None
	
def log_print(info):
	print(info)
	if tc_logfile != None:
		tc_logfile.write(info + "\n")
		tc_logfile.flush()

tc_output_dir=""
tc_cur_test = ""
tc_test_id = 0

def init(odir = "./results.d/"):
	global tc_output_dir
	tc_output_dir = odir + "_" + datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
	tc_output_dir = os.path.expanduser(tc_output_dir)
	os.system("mkdir -p " + tc_output_dir)
	global tc_logfile
	tc_logfile = open(tc_output_dir + "/log.txt", "w+")

def begin(name):
	global tc_test_id
	global tc_cur_test
	tc_cur_test = name
	tc_test_id += 1
	os.system("mkdir -p " + get_odir())
	log_print("\n===== Test #" + str(tc_test_id) + " - " + tc_cur_test + " started =====")

def end():
	global tc_cur_test
	log_print("\n===== Test #" + str(tc_test_id) + " - " + tc_cur_test + " completed =====")
	tc_cur_test = None

def get_odir():
	return tc_output_dir + "/" + tc_cur_test

SCHED_QUEUE = 1
SCHED_CPU = 2
SCHED_BEST = 4
SCHED_FEAT_WS = 1
def make_sched_flag(sched, args, feat = 0, fargs = 0):
	return (sched & 0xFF) | (args & 0xFF) << 8 | (feat & 0xFF) << 16 | (fargs & 0xFF) << 24

TUNE_RTSHARE = 2
TUNE_TFREQ = 1
def make_tune_flag(obj, val):
	return (obj & 0xFFFF) | (val & 0xFFFF) << 16 

def get_username():
    return pwd.getpwuid( os.getuid() )[0]

def remote_exec(srv, cmd, blocking=True, check=True):
	sub = []
	for s in srv:
		p = sp.Popen(["ssh " + s + " \"" + cmd +"\""], shell=True, stdout=sp.PIPE, stderr=sp.PIPE)
		sub.append(p)
	
	if blocking:
		for p in sub:
			p.wait()
			if check and p.returncode != 0:
				raise Exception("Command failed " + cmd)

	return sub

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
			line.strip()

			if len(line) == 0:
				break

			if exclude != None:
				for exc in exclude:
					if (exc != None) and (re.match(exc, line) != None):
						return True
			
			log_print("Error detected: proc idx - " + str(i) + " " + line)
			return False
			
	return True

def parse_hostfile(fp):
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
		if line in hosts:
			ret.append(hosts[line])
		else:
			ret.append(line)
	return ret

def get_cpuset_core(threads):
	ret = "cpuset -l 0-" + str(threads * 2 - 1) + " "
	return ret
