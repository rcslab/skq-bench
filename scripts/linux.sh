#!/bin/sh
kldload linux
kldload linux64
echo y | pkg install linux-c7
umount /compat/linux/proc
umount /compat/linux/sys
umount /compat/linux/dev/shm
mount -t linprocfs linprocfs /compat/linux/proc
mount -t linsysfs linsysfs /compat/linux/sys
mount -t tmpfs tmpfs /compat/linux/dev/shm
