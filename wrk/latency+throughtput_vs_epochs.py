#!/usr/bin/env python3.6

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys
import re

if len(sys.argv) > 1:
    df = pd.read_csv(sys.argv[1], skipinitialspace=True)
    if len(sys.argv) == 3:
        if sys.argv[1][len(sys.argv[1])-4: 4] == '.csv':
            fn = re.match(r'(^.+)\.csv$', sys.argv[1]).group(1)
        else:
            fn = sys.argv[1]

        if sys.argv[2] == '.':
            output_img_fn = fn + '.png'
        else:
            output_img_fn = sys.argv[2]
else:
    df = pd.read_csv("default.csv", skipinitialspace=True)

print(df)

latency90_mean = df['Latency-90P']
latency90_mean = latency90_mean.astype(np.float)
#latency90_mean = latency90_mean[0:50]

latency50_mean = df['Latency-50P']
latency50_mean = latency50_mean.astype(np.float)
#latency50_mean = latency50_mean[0:50]

throughput_mean = df['Avg-Requests']
throughput_mean = throughput_mean.astype(np.float)
#throughput_mean = throughput_mean[0:50]

latency_std = df['Latency-StDev'].unique()

x = np.arange(len(latency90_mean))
#x = x[0:50]

plt.figure(1)

fig, p50 = plt.subplots()
p50.set_ylabel('P50')
p50.plot(x, latency50_mean)
p50.plot(x, latency90_mean)

tp = p50.twinx()
tp.set_ylabel('Throughput')
tp.plot(x, throughput_mean)

plt.title('Latency - Throughtput')
plt.gcf().set_size_inches(30, 20)

fig.tight_layout()

plt.legend(loc='best')
if len(sys.argv) > 2:
    plt.savefig(output_img_fn)
plt.show()
