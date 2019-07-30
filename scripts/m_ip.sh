#!/bin/sh

ifconfig ixl0 up

HOSTCOUNT=$(hostname | cut -c8)
LIMIT=16
OFFSET=$(($(($LIMIT * $(($HOSTCOUNT - 1)))) + 1))
COUNT=0

while [ "$COUNT" -lt "$LIMIT" ]
do
        IPADDR=$(($OFFSET + $COUNT))
        ifconfig ixl0 alias 192.168.6.$IPADDR/24
#       ifconfig mce0 -alias 192.168.5.$IPADDR
        COUNT=$(($COUNT + 1))
done
