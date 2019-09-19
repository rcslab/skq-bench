#!/usr/bin/env python

import sys
import os
import csv

CLEAR = "\r\033[K"
RED = "\033[0;31m"
GREEN = "\033[0;32m"
YELLOW = "\033[0;34m"
NORMAL = "\033[0;39m"

FORMAT = "%-32s %20s %20s %10s %s%10s%s\n"

result = None

def write(str):
    sys.stdout.write(str)
    sys.stdout.flush()

def ParseData(filename):
    with open(filename) as fd:
        r = csv.reader(fd, skipinitialspace=True, delimiter=',')
        data = []
        for row in r:
            data.append(row)
        return data
    return None

def Lookup(tbl, name):
    for r in tbl:
        if r[0] == name:
            return r
    return None

def PrintRow(name, base, base_sd, new, new_sd, units):
    diff_str = ""
    color = GREEN
    if (base is not None) and (new is not None):
        diff = float(new)/float(base)-1.00
        diff_str = "{:3.2f}%".format(diff*100.0)
        if diff < 0:
            color = RED
    if base is None:
        base = "-"
        base_sd = ""
    else:
        base = base + "+/-" + base_sd
    if new is None:
        new = "-"
        new_sd = ""
    else:
        new = new + "+/-" + new_sd
    write(FORMAT % (name, base, new, units, color, diff_str, NORMAL));
    output.write("|%s|%s|%s|%s|%s|\n" % (name, base, new, units, diff_str))

def PrintTable(base, new):
    for r in base:
        nv = Lookup(new, r[0])
        if nv is None:
            PrintRow(r[0], r[1], r[2], None, None, r[3])
        else:
            PrintRow(r[0], r[1], r[2], nv[1], nv[2], r[3])
    for r in new:
        nv = Lookup(base, r[0])
        if nv is None:
            PrintRow(r[0], None, r[1], r[2], r[3])

if len(sys.argv) != 3:
    write("Usage: perfdiff.py [perftest_1.csv] [perftest_2.csv]\n")

base = ParseData(sys.argv[1])
new = ParseData(sys.argv[2])
output = open("perfresults.csv", "w")
output.write("=Performance Report=\n")
output.write("|Test|Base|New|Units|Change|\n")

write(FORMAT % ("Test", "Base", "New", "Units", "", "Change", ""))
write("------------------------------------------------------------------------------------------------\n")

PrintTable(base, new)
output.close()

