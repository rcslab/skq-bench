#!/usr/bin/env python3.6

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

df = pd.read_csv("test.csv", skipinitialspace=True)
print(df)

conns = df['Connections Count'].unique()

for c in conns:
    onedf = df.loc[df['Connections Count'] == c]
    plt.plot(onedf['Threads Count(Server)'],
             onedf['Events Count'],
             marker = 'o',
             linestyle = ':',
             label = c)

plt.title('Thread Scalability')
plt.xlabel('Threads')
plt.ylabel('Events Per Second')

plt.legend(loc='best')
plt.show()

