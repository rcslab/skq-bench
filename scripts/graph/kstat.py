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
import xml.etree.ElementTree as ET
import concurrent.futures as CF

num_bins = 1000
extra_pct = [50]

col = ["avg_latency", "total_time", "total_syscall", "total_events", "avg_events", "total_fallbacks", "total_mismatches", "total_worksteal"]

def gen_graph(fpath, data):
    dirname = os.path.dirname(fpath)
    fname = os.path.basename(fpath)
    dirname = dirname + "/" + fname + "_graphs"
    os.system("mkdir -p " + dirname)
    
    for ecol in col:
        tot = {}
        i = 0
        for rchild in data["dat"]:   
            for child in rchild:
                if child.tag == "kevq_dump":
                    for echild in child:
                        if echild.tag == "kevq" and echild.attrib[ecol] != None:
                            kevq_name = echild.attrib["ptr"]
                            if not (kevq_name in tot.keys()):
                                tot[kevq_name] = {}
                                tot[kevq_name]["ts"] = []
                                tot[kevq_name]["dat"] = []

                            tot[kevq_name]["ts"].append(data["ts"][i])
                            tot[kevq_name]["dat"].append(int(echild.attrib[ecol]))
            i = i + 1
        
        
        for key in tot.keys():
            df = pd.DataFrame(tot[key])
            df = df.sort_values("ts")
            plt.plot("ts", "dat", data = df, label=key, marker='o')

        plt.xlabel("Timestamp")
        plt.ylabel("Frequency")
        plt.title(fname + "_" + ecol)
        plt.legend()
        f = plt.gcf()
        f.set_size_inches(11.69, 8.27)
        f.savefig(dirname + "/" + ecol + ".png", dpi=160)
        print("Generated - " +  dirname + "/" + ecol + ".png")
        plt.clf()


def parse_file(f):
    lines = f.readlines()
    i = 0
    start = False
    ts = 0
    seg = ""
    data = {}
    data['ts'] = []
    data['dat'] = []
    ts = 0
    while i < len(lines):
        eline = lines[i]
        if eline.find("DUMP") != -1 or eline.find("Dump") != -1:
            if start:
                try:
                    data["dat"].append(ET.fromstring(seg))
                    data["ts"].append(ts)
                except:
                    None
                start = False
                seg = ""

            start = True
            ts = ts + 1
                    
        elif start:
            if eline.find("Userspace") == -1:
                seg += eline

        i = i + 1
    return data

def process_file(each_dir):
    print("Processing " + each_dir + " ...")

    try:
        f = open(each_dir, 'r')
        dat = parse_file(f)
    except:
        return -1

    gen_graph(each_dir, dat)
    return 0


executor = CF.ProcessPoolExecutor(max_workers=int(os.cpu_count() * 1.5))

def process_dir(rootdir):
    for subdir in os.listdir(rootdir):
        each_dir = os.path.join(rootdir, subdir)
        if os.path.isfile(each_dir):
            if each_dir.endswith(".kstat"):
                executor.submit(process_file, each_dir)
                #process_file(each_dir)
        else:
            process_dir(each_dir)

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
    executor.shutdown()

if __name__ == "__main__":
    main()