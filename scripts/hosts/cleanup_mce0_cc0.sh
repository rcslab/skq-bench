#!/bin/sh

srv_cc0="skylake1"
srv_ixl0=""
#"skylake2 skylake3 skylake4 skylake5 skylake6 skylake7 skylake8"

mce0() {
    # separate these functions because we might change kernel (reboot) without needing to recompile
    echo "$1"
    ssh $1 "sudo ifconfig mce0 delete $1/24; \
            sudo ifconfig mce0 down;"
    wait
    echo "$1 Done."
    echo ""
}

cc0() {
    echo "$1"
    ssh $1 "sudo ifconfig cc0 delete $1/24; \
            sudo ifconfig cc0 down;"
    wait
    echo "$1 Done."
    echo ""
}

i=0
for server in $srv_cc0
do
	i=$(expr $i + 1)
    cc0 "$server" "192.168.100.1$i" &
done

i=0
for server in $srv_ixl0
do
	i=$(expr $i + 1)
    mce0 "$server" "192.168.100.10$i" &
done

wait
