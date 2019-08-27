#!/usr/local/bin/bash
test_dir="/tmp/tests.d"
root=".."

servers=(localhost skylake1 skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8)

rsync_flags="-qvchar"

init() {
    echo "====================$1===================="
    echo "Performing initial setup..."
    ssh -t $1 "echo y | \
               sudo pkg install git dtrace-toolkit autotools gcc cmake scons libevent gengetopt rsync libzmq4; \
               sudo chmod 777 /tmp; \
               rm -rf /tmp/cppzmq; \
               rm -rf $test_dir; \
               git clone https://github.com/zeromq/cppzmq /tmp/cppzmq; \
               sudo cp /tmp/cppzmq/zmq.hpp /usr/local/include/"
    rsync $rsync_flags $root/ $1:$test_dir/
    ssh -t $1 "sudo $test_dir/scripts/sysctl.sh"
    echo "$1 Done."
    echo ""
}

i=0
while [ $i -lt ${#servers[@]} ]
do
	server=${servers[$i]}
	i=$(expr $i + 1)
    init "$server" &
done

wait
