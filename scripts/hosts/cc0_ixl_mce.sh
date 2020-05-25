#!/bin/sh
srv_cc0="skylake1"
srv_ixl0="skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8"
srv_mce0="sandybridge1 sandybridge2 sandybridge3 sandybridge4"

mce0() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "$1 -> $2"
    ssh $1 "
            sudo ifconfig mlxen0 inet $2/24; \
            sudo ifconfig mlxen0 up;"
    wait
    echo "$1 Done."
    echo ""
}

ixl0() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "$1 -> $2"
    ssh $1 "
            sudo ifconfig ixl1 inet $2/24 alias; \
            sudo ifconfig ixl1 up;"
    wait
    echo "$1 Done."
    echo ""
}

cc0() {
    echo "$1 -> $2"
    ssh $1 "sudo kldload if_cxgbe; \
        sudo bash /tmp/tests.d/scripts/hosts/cc0_pin.sh; \
        sudo ifconfig cc0 inet $2/24; \
        sudo ifconfig cc0 up;"
    wait
    echo "$1 Done."
    echo ""
}

i=100

for server in $srv_cc0
do
	i=$(expr $i + 1)
    cc0 "$server" "192.168.100.$i" &
done

for server in $srv_ixl0
do
	i=$(expr $i + 1)
    ixl0 "$server" "192.168.100.$i" &
done


for server in $srv_mce0
do
	i=$(expr $i + 1)
    mce0 "$server" "192.168.100.$i" &
done
wait
