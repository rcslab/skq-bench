#!/bin/sh
test_dir="/tmp/tests.d/"
root="../"

servers="skylake1" 

rsync_flags="-qvchar"

ssh_cmd="ssh -o StrictHostKeyChecking=no -t "

init() {
    echo "====================$1===================="
    echo "Performing initial setup..."
    $ssh_cmd $1 "echo y | \
               sudo pkg install gmake libsodium bash git dtrace-toolkit autotools gcc cmake libevent gengetopt rsync libzmq4 protobuf; \
               sudo chmod 777 /tmp; \
               sudo rm -rf /tmp/cppzmq; \
               sudo rm -rf $test_dir; \
               git clone https://github.com/zeromq/cppzmq /tmp/cppzmq; \
               sudo kldload hwpmc; \
               sudo cp /tmp/cppzmq/zmq.hpp /usr/local/include/; \
               mkdir -p /tmp/charm; \
               sudo umount /tmp/charm; \
               sudo mount charm.rcs.uwaterloo.ca:/home /tmp/charm; "
               #sudo mdconfig -a -t swap -s 4g -u 1; sudo newfs -U md1; sudo mkdir -p /mnt/md; sudo mount /dev/md1 /mnt/md; sudo cp -r /home/oscar/rocksdb.db /mnt/md; sudo chmod -R 777 /mnt; "
    rsync $rsync_flags $root/ $1:$test_dir/
    $ssh_cmd $1 "sudo $test_dir/scripts/sysctl.sh; \
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
