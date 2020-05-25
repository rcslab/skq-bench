#!/bin/sh

srv_ixl0="skylake1 skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8"
srv_mce0="sandybridge1 sandybridge2 sandybridge3 sandybridge4"

mce0() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "$1 -> $2"
    ssh $1 "
            sudo ifconfig mlxen0 inet $2/24 delete; \
            sudo ifconfig mlxen0 up;"
    wait
    echo "$1 Done."
    echo ""
}

ixl0() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "$1 -> $2"
    ssh $1 "
            sudo ifconfig ixl1 inet $2/24 delete; \
            sudo ifconfig ixl1 up;"
    wait
    echo "$1 Done."
    echo ""
}

i=100


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
