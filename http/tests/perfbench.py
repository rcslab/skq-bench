#!/usr/bin/env python

import sys
import os
import time
import subprocess

TIMEOUT = 10.0

CLEAR = "\r\033[K"
RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[0;34m"
NORMAL = "\033[0;39m"

FORMAT = "%-32s [ %s%-9s"+ NORMAL + " ]"
TFORMAT = "%-32s [ %s%-9s"+ NORMAL + " ] %-10.6f"

all_tests = []
for f in sorted(os.listdir('tests/')):
    if f.endswith("_perf.c"):
        all_tests.append(os.path.basename(f[0:-7]))
    if f.endswith("_perf.cc"):
        all_tests.append(os.path.basename(f[0:-8]))

tests = [ ]
failed = [ ]
disabled = [ ]

try:
    os.unlink("tests/perftest.csv")
except OSError:
    pass
perfcsv = open("tests/perftest.csv", "w+")

def write(str):
    sys.stdout.write(str)
    sys.stdout.flush()

def DeleteFile(name):
    try:
        os.unlink(name)
    except OSError:
        pass

def CleanTest(name):
    DeleteFile(name + "_perf.core")

def ReportError(name):
    write(CLEAR)
    write(FORMAT % (name, RED, "Failed") + "\n")
    failed.append(name)

def ReportTimeout(name):
    write(CLEAR)
    write(FORMAT % (name, RED, "Timeout") + "\n")
    failed.append(name)

def ReportDisabled(name):
    write(CLEAR)
    write(FORMAT % (name, YELLOW, "Disabled") + "\n")
    disabled.append(name)

def ReadDisabledList():
    with open('DISABLED') as f:
        disabled_tests = f.read().splitlines()
    autogenerate_list = map(lambda x: x.strip(), disabled_tests)
    autogenerate_list = filter(lambda x:not x.startswith('#'), disabled_tests)
    return disabled_tests

def Run(tool, name):
    start = time.time()
    t = subprocess.Popen(["../build/tests/" + name + "_perf"],
                         stdout=perfcsv)
    while 1:
        t.poll()
        if t.returncode == 0:
            return (time.time() - start)
        if t.returncode != None:
            ReportError(name)
            return None
        if (time.time() - start) > TIMEOUT:
            t.kill()
            ReportTimeout(name)
            return None

def RunTest(name):
    write(FORMAT % (name, NORMAL, "Running"))

    # Normal
    norm_time = Run([], name)
    if norm_time is None:
        return

    write(CLEAR)
    write(TFORMAT % (name, GREEN, "Completed", norm_time))
    write("\n")

basedir = os.getcwd()
if (basedir.split('/')[-1] != 'tests'):
    os.chdir('tests')

if len(sys.argv) > 1:
    for t in sys.argv[1:]:
        if all_tests.count(t) == 0:
            write("Test '%s' does not exist!\n" % (t))
            sys.exit(255)
        tests.append(t)
else:
    tests = all_tests

write("%-32s   %-9s   %-10s\n" %
        ("Test", "Status", "Normal"))
write("------------------------------------------------------------------------------\n")
disabled_tests = ReadDisabledList()
for t in tests:
    if t in disabled_tests:
        ReportDisabled(t)
        continue
    CleanTest(t)
    RunTest(t)

if len(failed) != 0:
    write(str(len(failed)) + " tests failed\n")
if len(disabled) != 0:
    write(str(len(disabled)) + " tests disabled\n")

sys.exit(len(failed))

