#!/bin/sh

sysctl kern.kq_calloutmax=65536
sysctl net.inet.ip.portrange.hifirst=10000
sysctl net.inet.tcp.msl=2500
sysctl kern.ipc.soacceptqueue=65536
