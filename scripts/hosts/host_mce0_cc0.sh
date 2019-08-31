#!/bin/sh
test_dir="/tmp/tests.d"

srv_cc0="skylake1"
srv_mce0="skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8"

mce0() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "$1 -> $2"
    ssh $1 "sudo kldload mlx5; \
            sudo kldload mlx5en; \
            sudo bash /usr/bin/set_irq_affinity_freebsd.sh mce0 NUMA 0; \
            sudo ifconfig mce0 inet $2/24; \
            sudo ifconfig mce0 up;"
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
