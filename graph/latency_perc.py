#!usr/bin/env python3

import sys
import csv
import matplotlib.pyplot as plt
import numpy as np
import re

headers = None
delay_val = []
latency50_val = []
latency95_val = []

def computePercentile(r):
    total = sum(list(map(int,r)))
    offset = 0
    perc50 = 0
    perc50_offset = np.inf
    perc95 = 0
    perc95_offset = np.inf

    for i in range(1, len(r)):
        print(headers[i] + ": {:2.2%}".format(offset / total))
        if abs(offset/total*100 - 50) < perc50_offset:
            perc50_offset = abs(offset/total*100 - 50)
            perc50 = i

        if abs(offset/total*100-95) < perc95_offset:
            perc95_offset = abs(offset/total*100 - 95)
            perc95 = i
        offset += int(r[i])

    nl = int(re.match(r'(\d*)-(\d*)', headers[perc50]).group(1))
    nr = int(re.match(r'(\d*)-(\d*)', headers[perc50]).group(2))
    latency50_val.append( str(int((nl+nr)/2)) )

    nl = int(re.match(r'(\d*)-(\d*)', headers[perc95]).group(1))
    nr = int(re.match(r'(\d*)-(\d*)', headers[perc95]).group(2))
    latency95_val.append( str(int((nl+nr)/2)) )

    delay_val.append( r[0] )

if len(sys.argv) > 1:
    f = open(sys.argv[1], 'r')
else:
    f = open('resp.csv', 'r')

csvreader = csv.reader(f, delimiter=',', skipinitialspace=True)
for row in csvreader:
    if headers == None:
        headers = row
    else:
        computePercentile(row)

plt.figure(1)

plt.title('Latency')
plt.xlabel('Delay on Server Side')
plt.ylabel('Round Trip Time')

plt.plot(delay_val, latency50_val, label='50 Percentile')
plt.plot(delay_val, latency95_val, label='95 Percentile')
plt.legend()

plt.show()


