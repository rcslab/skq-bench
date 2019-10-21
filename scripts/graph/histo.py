#!/usr/bin/env python3.6

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.mlab as mlab
import numpy as np
import sys
import re
import os
import json
import getopt
import math
import concurrent.futures as CF

num_bins = 1000
extra_pct = []

def saveplot(files, data):
    plt.hist(data, num_bins)
    plt.xlabel("Latency")
    plt.ylabel("Frequency")
    plt.title(os.path.basename(files))
    f = plt.gcf()
    f.set_size_inches(11.69, 8.27)
    f.savefig(files + ".png", dpi=160)
    plt.clf()
    print("Generated - " + files + ".png")

def output_extra_percentile(data):
    a = np.array(data)
    for pct in extra_pct:
        p = np.percentile(a, pct)
        print(str(pct) + "th: " + str(p))

def sanitize(data):
    ret = []
    a = np.array(data)
    p99 = np.percentile(a, 99)
    for i in data:
        if i <= p99:
            ret.append(i)
    return ret


executor = CF.ProcessPoolExecutor(max_workers=int(os.cpu_count()))

def process_file(each_dir):
    try:
        print("Processing " + each_dir + " ...")
        data = memparse.parse_mut_sample(each_dir)[1]
        output_extra_percentile(data)   
        sdata = sanitize(data)       
        saveplot(each_dir, sdata)
    except:
        None

def process_dir(rootdir):
    for subdir in os.listdir(rootdir):
        each_dir = os.path.join(rootdir, subdir)
        if os.path.isfile(each_dir):
            if each_dir.endswith("sample.txt") or each_dir.endswith(".sample"):
                #executor.submit(process_file, each_dir)
                process_file(each_dir)
        else:
            process_dir(each_dir)

def main():    
    datdir = None
    options = getopt.getopt(sys.argv[1:], 'd:')[0]

    for opt, arg in options:
        if opt in ('-d'):
            datdir = arg

    if datdir == None:
        datdir = "/home/oscar/projs/kqsched/scripts/pingpong/results.d/sample"
        #raise Exception("Must specify -d parameter")

    process_dir(datdir)
    executor.shutdown()

if __name__ == "__main__":
    main()
    