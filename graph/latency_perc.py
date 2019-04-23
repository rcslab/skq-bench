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
        if abs(offset/total*100-50) < perc50_offset:
            perc50_offset = abs(offset/total*100 - 50)
            perc50 = i

        if abs(offset/total*100-95) < perc95_offset:
            perc95_offset = abs(offset/total*100 - 95)
            perc95 = i
        offset += int(r[i])
        print(headers[i] + ": {:2.2%}".format(offset / total))

    nl = int(re.match(r'(\d*)-(\d*)', headers[perc50]).group(1))
    if len(re.match(r'(\d*)-(\d*)', headers[perc50]).group(2)) == 0:
        nr = 99999999
    else:
        nr = int(re.match(r'(\d*)-(\d*)', headers[perc50]).group(2))
    latency50_val.append( int((nl+nr)/2) )

    nl = int(re.match(r'(\d*)-(\d*)', headers[perc95]).group(1))
    if len(re.match(r'(\d*)-(\d*)', headers[perc95]).group(2)) == 0:
        nr = 99999999
    else:
        nr = int(re.match(r'(\d*)-(\d*)', headers[perc95]).group(2))
    latency95_val.append( int((nl+nr)/2) )

    delay_val.append( int(r[0]) )

if len(sys.argv) > 1:
    f = open(sys.argv[1], 'r')
else:
    f = open('resp.csv', 'r')

csvreader = csv.reader(f, delimiter=',', skipinitialspace=True)

header_line = 0
datasetNum = 0
dsnFlag = 0
totalDataRow = 0

for row in csvreader:
    if headers == None:
        headers = row
    else:
        computePercentile(row)
        totalDataRow += 1

    if dsnFlag == 0:
        if header_line == 0:
            header_line = 1
        else:
            datasetNum+=1
            if int(row[0])==0:
                dsnFlag = 1

datasetNumCount = int(totalDataRow / datasetNum)

plt.figure(1)

plt.title('Latency')
plt.xlabel('Delay on Server Side')
plt.ylabel('Round Trip Time')

if len(sys.argv) > 2:
    n_to_plt = int(sys.argv[2])

i = n_to_plt
plt.plot(delay_val[i*datasetNum:(i+1)*datasetNum-1], latency50_val[i*datasetNum:(i+1)*datasetNum-1], label='50 Percentile')
plt.plot(delay_val[i*datasetNum:(i+1)*datasetNum-1], latency95_val[i*datasetNum:(i+1)*datasetNum-1], label='95 Percentile')

print("Each sample has " + str(datasetNum) + " sets and in total " + str(datasetNumCount) + " sets and " + str(totalDataRow) + " lines of input")

plt.legend()
plt.show()


