#!/bin/sh
test_dir="/tmp/tests.d"
root=".."

servers="skylake1 skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8 localhost"

rsync_flags="-qvchar"

compile() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "====================$1===================="
    echo "Syncing directories..."    
    rsync $rsync_flags $root/ $1:$test_dir/
    echo "Compiling..."
    ssh $1 "cd $test_dir/pingpong; make clean; make all" &
    ssh $1 "cd $test_dir/mutilate; scons" &
    ssh $1 "cd $test_dir/memcached; ./configure ; make clean; make all" &
    ssh $1 "rm -rf $test_dir/evcompat/build; mkdir -p $test_dir/evcompat/build; cd $test_dir/evcompat/build; cmake ../; make"
    ssh $1 "cd $test_dir/mem; ./configure ;make clean; make all" &
    wait
    echo "$1 Done."
    echo ""
}

i=0
for server in $servers
do
	i=$(expr $i + 1)
    compile "$server" &
done

wait
