#!/usr/bin/env python3

import sys
import csv

headers = None

def computePercentile(r):
    total = sum(list(map(int,r)))
    offset = 0
    for i in range(len(r)):
        print(headers[i] + ": {:2.2%}".format(offset / total))
        offset += int(r[i])

f = open(sys.argv[1], 'r')
csvreader = csv.reader(f, delimiter=',', skipinitialspace=True)
for row in csvreader:
    if headers == None:
        headers = row
    else:
        computePercentile(row)

