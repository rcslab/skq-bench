#!/usr/local/bin/bash
test_dir="/tmp/tests.d"
init="$1"
root="../../kqsched"

servers=(skylake1 skylake2 skylake3)

rsync_flags="-qvchar"

compile() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "====================$1===================="
    echo "Syncing directories..."    
    rsync $rsync_flags $root/ $1:$test_dir/
    echo "Compiling..."
    ssh $1 "cd $test_dir; make clean; make all" &
    ssh $1 "cd $test_dir/mutilate; scons" &
    ssh $1 "cd $test_dir/memcached; ./autogen.sh; ./configure ; make clean; make all" &
    ssh $1 "rm -rf $test_dir/evcompat/build; mkdir -p $test_dir/evcompat/build; cd $test_dir/evcompat/build; cmake ../; make"
    ssh $1 "cd $test_dir/mem;  ./autogen.sh; ./configure ;make clean; make all" &
    echo "$1 Done."
    echo ""
}

init() {
    echo "====================$1===================="
    echo "Performing initial setup..."
    ssh -t $1 "sudo pkg install autotools gcc cmake scons libevent gengetopt; sudo $test_dir/scripts/m_ip.sh; sudo $test_dir/scripts/sysctl.sh"
    echo "$1 Done."
    echo ""
}

i=0
while [ $i -lt ${#servers[@]} ]
do
	server=${servers[$i]}
	i=$(expr $i + 1)
    if [ "$init" = "init" ]; then
        init "$server"
    else
        compile "$server" &
    fi
done

# wait for all child processes
for job in `jobs -p`
do
	wait $job
done
