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
    df = pd.read_csv("test.csv", skipinitialspace=True)

print(df)
kq = df['KQueue Count'].unique()
threads = df['Threads Count(Server)'].unique()

plt.figure(1)

for k in kq:
    kdf = df.loc[df['KQueue Count'] == k]
    for t in threads:
        onedf = kdf.loc[kdf['Threads Count(Server)'] == t]
        conns = onedf['Connections Count'].unique()
        y_mean = []
        y_std = []
        for c in conns:
            y = onedf.loc[onedf['Connections Count'] == c]['Events Count']
            y_mean.append(y.mean())
            y_std.append(y.std())
        plt.errorbar(conns, y_mean, yerr = y_std,
                fmt='o', #marker = 'o',
                linestyle = ':',
                capsize = 5,
                label = str(k) + "x" + str(t))

plt.title('Connection Scalability')
plt.xlabel('Connections')
plt.ylabel('Events Per Second')
plt.gcf().set_size_inches(30, 20)

plt.legend(loc='best')
if len(sys.argv) > 2:
    plt.savefig(output_img_fn)
plt.show()
