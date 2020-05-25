#!/bin/sh
test_dir="/tmp/tests.d"
root=".."
servers="skylake1 skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8 sandybridge1 sandybridge2 sandybridge3 sandybridge4" 

rsync_flags="-qvchar"

ssh_cmd="ssh -o StrictHostKeyChecking=no -t "

compile() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "====================$1===================="
    echo "Syncing directories..."    
    rsync $rsync_flags $root/ $1:$test_dir/
    echo "Compiling..."
    $ssh_cmd $1 "cd $test_dir/pingpong; rm -rf build; mkdir build; cd build; cmake ../; make" &
    $ssh_cmd $1 "cd $test_dir/mutilate; scons" &
    $ssh_cmd $1 "cd $test_dir/memcached; ./configure ; make clean; make all" &
    $ssh_cmd $1 "rm -rf $test_dir/evcompat/build; mkdir -p $test_dir/evcompat/build; cd $test_dir/evcompat/build; cmake ../; make" &
    $ssh_cmd $1 "cd $test_dir/scaled; mkdir build; cd build; cmake ../; make" &
    $ssh_cmd $1 "cd $test_dir/mem; ./configure ;make clean; make all" &
    if [ "$1" = "skylake1" ]; then
        $ssh_cmd $1 "cd $test_dir/celestis; scons -c; scons -j 16" &
    fi;
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
