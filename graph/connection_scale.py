#!/usr/bin/env python3.6

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

df = pd.read_csv("test.csv", skipinitialspace=True)
print(df)

threads = df['Threads Count(Server)'].unique()

for t in threads:
    onedf = df.loc[df['Threads Count(Server)'] == t]
    plt.plot(onedf['Connections Count'],
             onedf['Events Count'],
             marker = 'o',
             linestyle = ':',
             label = t)

plt.title('Connection Scalability')
plt.xlabel('Connections')
plt.ylabel('Events Per Second')

plt.legend(loc='best')
plt.show()

