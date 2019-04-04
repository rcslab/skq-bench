#!/usr/bin/env python3.6

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import sys

if len(sys.argv) > 1:
    df = pd.read_csv(sys.argv[1], skipinitialspace=True)
else:
    df = pd.read_csv("test.csv", skipinitialspace=True)
print(df)

kq = df['KQueue Count'].unique()
conns = df['Connections Count'].unique()

for k in kq:
    kdf = df.loc[df['KQueue Count'] == k]
    for c in conns:
        onedf = kdf.loc[kdf['Connections Count'] == c]
        threads = onedf['Threads Count(Server)'].unique()
        y_mean = []
        y_std = []
        for t in threads:
            y = onedf.loc[onedf['Threads Count(Server)'] == t]['Events Count']
            y_mean.append(y.mean())
            y_std.append(y.std())
        plt.errorbar(threads, y_mean, yerr = y_std,
                fmt='o', #marker = 'o',
                linestyle = ':',
                capsize = 5,
                label = str(k) + "x" + str(c))

plt.title('Thread Scalability')
plt.xlabel('Threads')
plt.ylabel('Events Per Second')

plt.legend(loc='best')
plt.show()

