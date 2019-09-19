#!/bin/sh

rm -rf .cleanbuild
mkdir .cleanbuild
GIT_WORK_TREE=.cleanbuild git checkout -f origin/master .
cp Local.sc .cleanbuild/
cd .cleanbuild
scons
scons perfbench

cd ..
scons
scons perfbench

cd tests
./perfdiff.py ../.cleanbuild/tests/perftest.csv perftest.csv
cd ..

