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
import concurrent.futures as CF
import shutil

def process_dir(rootdir):
    ret = []
    print("Processing " + rootdir + " ...")
    for subdir in os.listdir(rootdir):
        each_dir = os.path.join(rootdir, subdir)
        if os.path.isfile(each_dir):
            if each_dir.endswith(".png"):
                os.remove(each_dir)
                print("RM - " + each_dir)
        elif each_dir.endswith("_graphs"):
            shutil.rmtree(each_dir)
            print("RM - " + each_dir)
        else:
            process_dir(each_dir)
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

    process_dir(datdir)

if __name__ == "__main__":
    main()