#!/usr/bin/env python3.6

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import re
import os
import json
import pmcparse as pp
import getopt
import math
import concurrent.futures as CF

def process_dir(rootdir):
    ret = []
    print("Processing " + rootdir + " ...")
    for subdir in os.listdir(rootdir):
        each_dir = os.path.join(rootdir, subdir)
        if os.path.isfile(each_dir) and each_dir.endswith(".pmcstat"):
            output = None
            try:
                output = open(each_dir, 'r').read()
                eachobj = pp.parse_pmc_output(output)
                eachobj.qps = int(subdir.strip(".pmcstat").strip("l"))
                print("Processed - " + each_dir)
                ret.append(eachobj)
            except:
                print("Unrecognized format - " + each_dir)
        
    print("")
    return ret

def process_columns(dat: []):
    dct = {}
    # dct = {column -> {sched -> [{df1}, {df2}]}}

    for edat in dat:
        sched : str = edat[0]
        pdata = edat[1]
        for epdata in pdata:
            qps = epdata.qps
            for pmcdata in epdata.data:
                col = pmcdata[0]

                # lazily allocate structs
                if col not in dct:
                    dct[col] = {}
                if sched not in dct[col]:
                    dct[col][sched] = []
                    dct[col][sched].append({})
                    dct[col][sched][0]["qps"] = []
                    dct[col][sched][0]["times"] = []
                    if (pmcdata[3] != None):
                        dct[col][sched].append({})
                        dct[col][sched][1]["qps"] = []
                        dct[col][sched][1][pmcdata[3]] = []

                # keep val
                dct[col][sched][0]["qps"].append(qps)
                dct[col][sched][0]["times"].append(pmcdata[1])

                if (pmcdata[3] != None):
                    # keep rel
                    dct[col][sched][1][pmcdata[3]].append(pmcdata[2])
                    dct[col][sched][1]["qps"].append(qps)
    
    return dct

def main():
    datdir = None
    options = getopt.getopt(sys.argv[1:], 'd:')[0]

    for opt, arg in options:
        if opt in ('-d'):
            datdir = arg

    if datdir == None:
        datdir = "/home/oscar/results.d/rss/sample"
        #raise Exception("Must specify -d parameter")

    dat = []

    for sched in os.listdir(datdir):
        each_dir = os.path.join(datdir, sched)
        if not os.path.isfile(each_dir):
            dat.append([sched, process_dir(each_dir)])
    
    dct = process_columns(dat)

    for col in dct:
        num_subplot = 1
        # get # subplot
        for sched in dct[col]:
            if len(dct[col][sched]) > 1:
                num_subplot = 2
            break

        fig, rax = plt.subplots(2, 1)

        for i in range(0, num_subplot):
            eax = rax[i]
            for sched in dct[col]:
                df = pd.DataFrame(dct[col][sched][i])
                df = df.sort_values('qps')

                other_ax = None
                # get the name of the other ax
                for k in dct[col][sched][i]:
                    if k != "qps":
                        other_ax = k

                eax.set_ylabel(other_ax)
                eax.set_xlabel("qps")
                eax.plot('qps', other_ax, data=df, label=sched)
            eax.set_title(col + "[" + str(i) + "]")
            eax.legend()

        fig.set_size_inches(23.4, 16.5)
        plt.savefig(datdir + "/" + col + ".png", dpi=300)
        #plt.show()

    # dct = {column, {sched, [{df1}, {df2}]}}

if __name__ == "__main__":
    main()
