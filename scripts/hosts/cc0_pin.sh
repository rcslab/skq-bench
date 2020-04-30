#!/bin/sh 

CPU=0

for IRQ in `/usr/bin/vmstat -ai | /usr/bin/sed -nE "/t6nex0:0a/s/irq([[:digit:]]+):.*/\1/p"`;
do
    echo /usr/bin/cpuset -l `expr ${CPU} + 1` -x $IRQ
	/usr/bin/cpuset -l `expr ${CPU} + 1` -x $IRQ
	CPU=$((CPU + 2))
done

CPU=0

for IRQ in `/usr/bin/vmstat -ai | /usr/bin/sed -nE "/t6nex0:1a/s/irq([[:digit:]]+):.*/\1/p"`;
do
    echo /usr/bin/cpuset -l `expr ${CPU} + 1` -x $IRQ
	/usr/bin/cpuset -l `expr ${CPU} + 1` -x $IRQ
	CPU=$((CPU + 2))
done

#/usr/bin/cpuset -l 23 -x 117
