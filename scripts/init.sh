#!/bin/sh
test_dir="/tmp/tests.d/"
root="../"

servers="skylake1 skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8 localhost"


rsync_flags="-qvchar"

init() {
    echo "====================$1===================="
    echo "Performing initial setup..."
    ssh -t $1 "echo y | \
               sudo pkg install bash git dtrace-toolkit autotools gcc cmake scons libevent gengetopt rsync libzmq4; \
               sudo chmod 777 /tmp; \
               rm -rf /tmp/cppzmq; \
               rm -rf $test_dir; \
               git clone https://github.com/zeromq/cppzmq /tmp/cppzmq; \
               sudo cp /tmp/cppzmq/zmq.hpp /usr/local/include/; \
               mkdir -p /tmp/charm; \
               sudo umount /tmp/charm;
               sudo mount charm.rcs.uwaterloo.ca:/home /tmp/charm;"
    rsync $rsync_flags $root/ $1:$test_dir/
    ssh -t $1 "sudo $test_dir/scripts/sysctl.sh; \
               sudo $test_dir/scripts/linux.sh; \
               tar -xf /tmp/charm/oscar/memcached_linox.tar -C $test_dir/; \
               sudo cp $test_dir/libevent*.so.5 /compat/linux/lib64/;"
    echo "$1 Done."
    echo ""
}

i=0
for server in $servers
do
	i=$(expr $i + 1)
    init "$server" &
done

wait
