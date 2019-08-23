#!/usr/local/bin/bash
test_dir="/tmp/tests.d"
root=".."
cleanup=$1
ccs=(skylake1)
servers=(skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8)

rsync_flags="-qvchar"

mce0() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "$1 -> $2"
    ssh $1 "sudo kldload mlx5; sudo ifconfig mce0 inet $2/24; sudo ifconfig mce0 up"
    wait
    echo "$1 Done."
    echo ""
}

cc0() {
    echo "$1 -> $2"
    ssh $1 "sudo kldload if_cxgbe; sudo ifconfig cc0 inet $2/24; sudo ifconfig cc0 up"
    wait
    echo "$1 Done."
    echo ""
}

i=0
while [ $i -lt ${#servers[@]} ]
do
	server=${servers[$i]}
	i=$(expr $i + 1)
    mce0 "$server" "192.168.100.1$i"
done

i=0
while [ $i -lt ${#ccs[@]} ]
do
	server=${ccs[$i]}
	i=$(expr $i + 1)
    cc0 "$server" "192.168.100.10$i"
done

wait
