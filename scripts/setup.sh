#!/usr/local/bin/bash
init=$1
root="../../kqsched"
test_dir=tests.d

servers=(skylake1 skylake2 skylake3)

rsync_flags="-qvchar -e "

i=0
while [ $i -lt ${#servers[@]} ]
do
	server=${servers[$i]}
	i=$(expr $i + 1)
    echo "====================$server===================="
    if [ "$init" = "init" ]; then
        echo "Performing initial setup..."
        ssh $server "su -c './$test_dir/m_ip.sh; ./$test_dir/sysctl.sh'"
    else
        # separate these functions because we might change kernel (reboot) without needing to recompile
        echo "Syncing directories..."    
        rsync $rsync_flags $root/ $server:~/$test_dir/
        echo "Compiling..."
        ssh $server "cd ~/$test_dir && make clean && make -j8"
    fi
	echo "Done."
    echo ""
done
