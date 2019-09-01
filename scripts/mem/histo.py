#!/usr/bin/env python3.6

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.mlab as mlab
import numpy as np
import sys
import re
import os
import json
import memparse
import getopt
import math

num_bins = 1000
extra_pct = [50]

def parse_file(f):
    ret = []
    lines = f.readlines()
    for line in lines:
        entry = line.split()
        if len(entry) != 2:
            raise Exception("Unrecognized line: " + line)
        ret.append(float(entry[1]))
    return ret

def saveplot(f, data):
    plt.hist(data, num_bins)
    plt.xlabel("Latency")
    plt.ylabel("Frequency")
    plt.title(os.path.basename(f))
    f = plt.gcf()
    f.set_size_inches(11.69, 8.27)
    f.savefig(f + ".png", dpi=160)
    plt.clf()
    print("Generated - " + f + ".png")

def output_extra_percentile(data):
    a = np.array(data)
    for pct in extra_pct:
        p = np.percentile(a, pct)
        print(str(pct) + "th: " + p)

        

def sanitize(data):
    ret = []
    a = np.array(data)
    p99 = np.percentile(a, 99)
    for i in data:
        if i <= p99:
            ret.append(i)
    return ret

def process_dir(rootdir):
    print("Processing " + rootdir + " ...")
    for subdir in os.listdir(rootdir):
        each_dir = os.path.join(rootdir, subdir)
        if os.path.isfile(each_dir):
            f = open(each_dir, 'r')
            try:
                data = parse_file(f)
                output_extra_percentile(each_dir, data)   
                sdata = sanitize(data)       
                saveplot(each_dir, sdata)
            except:
                None
        else:
            process_dir(each_dir)
        print("")

def main():    
    datdir = None
    options = getopt.getopt(sys.argv[1:], 'd:')[0]

    for opt, arg in options:
        if opt in ('-d'):
            datdir = arg

    if datdir == None:
        datdir = "/home/oscar/projs/kqsched/scripts/mem/results.d/sample"
        #raise Exception("Must specify -d parameter")

    process_dir(datdir)

main()