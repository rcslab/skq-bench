#!/usr/bin/env python3.6

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import re
import os
import json
import memparse
import getopt
import math

col_to_graph = ["avg", "90th", "95th", "99th"]
num_col = 1

def process_dir(rootdir):
    ret = []
    print("Processing " + rootdir + " ...")
    for subdir in os.listdir(rootdir):
        each_dir = os.path.join(rootdir, subdir)
        if os.path.isfile(each_dir):
            print("- " + subdir)
            output = None
            with open(each_dir, 'r') as file:
                output = file.read()
            eachobj = memparse.parse(output)
            ret.append(eachobj)
    return ret

def main():    
    datdir = None
    options = getopt.getopt(sys.argv[1:], 'd:')[0]

    for opt, arg in options:
        if opt in ('-d'):
            datdir = arg

    if datdir == None:
        datdir = "/home/oscar/projs/kqsched/scripts/mem/results.d/sample"
        #raise Exception("Must specify -d parameter")

    dat = {}

    for subdir in os.listdir(datdir):
        each_dir = os.path.join(datdir, subdir)
        if not os.path.isfile(each_dir):
            dat[subdir] = process_dir(each_dir)
    
    fig, rax = plt.subplots(math.ceil((len(col_to_graph)) / num_col), num_col)

    idx = 0
    for col in col_to_graph:
        print("Generating graph for " + col + " ...")
        eax = rax[idx]
        for sched in dat:
            df_dict = {}
            df_dict['qps'] = []
            df_dict['lat'] = []
            # extract the curve corresponding to col from each subdir 
            edat = dat[sched]
            # edat contains all data in that directory
            for ememdat in edat:
                df_dict['qps'].append(ememdat.qps)
                elat = ememdat.dat[col]
                if elat == None:
                    raise Exception(sched + " doesn't have col " + col)
                df_dict['lat'].append(elat)
            df = pd.DataFrame(df_dict)
            df = df.sort_values('qps')
            eax.set_yscale("log")
            eax.plot('qps', 'lat', data = df, label=sched, marker='o')
        
        eax.set_title(col)
        idx = idx + 1
    fig.set_size_inches(11.69, 8.27)
    plt.legend()
    plt.savefig(datdir + "/graph.png", dpi=600)
    plt.show()

main()