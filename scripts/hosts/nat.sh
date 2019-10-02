#!/bin/sh
test_dir="/tmp/tests.d"

srv_mce0="skylake1 skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8"

mce0() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "$1 -> $2"
    ssh $1 "
            sudo ifconfig ixl1 inet $2/24; \
            sudo ifconfig ixl1 up;"
    wait
    echo "$1 Done."
    echo ""
}

i=0
for server in $srv_mce0
do
	i=$(expr $i + 1)
    mce0 "$server" "192.168.2.13$i" &
done

wait
