#!/bin/sh
test_dir="/tmp/tests.d"

srv_cc0="skylake1"
srv_mce0="skylake2 skylake3 skylake5 skylake6 skylake7 skylake8"

cnt=0

mce0() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "$1 -> $2"
    ssh $1 "sudo ifconfig ixl1.4004 destroy; \
            sudo ifconfig ixl1.4004 create vlan 4004 vlandev ixl1; \
            sudo ifconfig ixl1.4004 inet $2/24; \
            sudo ifconfig ixl1.4004 up;"
    wait
    echo "$1 Done."
    echo ""
}

cc0() {
    echo "$1 -> $2"
    ssh $1 "sudo kldload if_cxgbe; \
        sudo bash $test_dir/scripts/hosts/cc0_pin.sh; \
        sudo ifconfig cc0 inet $2/24; \
        sudo ifconfig cc0 up;"
    wait
    echo "$1 Done."
    echo ""
}

i=0
for server in $srv_cc0
do
	i=$(expr $i + 1)
    cc0 "$server" "192.168.100.10$i" &
done

i=0
for server in $srv_mce0
do
	i=$(expr $i + 1)
    mce0 "$server" "192.168.100.1$i" &
done

wait
