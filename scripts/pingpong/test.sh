#!/usr/local/bin/bash
# directories
result_dir=results.d
test_dir="/tmp/tests.d"
server_file=mtserver/kqserver
client_file=mtclient/kqclient

server="skylake1" # skylake1
client1="skylake2" # skylake2
client2="skylake3" # skylake3

full_case="test.case"
test_case="half.case"

echo "Creating local directories"
rm -rf $result_dir
mkdir $result_dir

pretest() {
	ssh $server -f "$test_dir/$server_file >> $test_dir/test.log 2>&1"
	ssh $client1 -f "$test_dir/$client_file >> $test_dir/test.log 2>&1"
	ssh $client2 -f  "$test_dir/$client_file >> $test_dir/test.log 2>&1"
	sleep 1
}

posttest() {
	ssh $server "killall kqserver"
	ssh $client1 "killall kqclient"
	ssh $client2 "killall kqclient"
}

params=("legacy" " " $full_case  \
        "rr" "-m 0" $test_case  \
        "queue" "-m 1"  $test_case \
		"cqueue" "-m 2" $test_case \
		"bon" "-m 8" $test_case \
		"ws" "-m 4" $test_case \
		"ws_bon" "-m 12" $test_case \
		"ws_queue" "-m 5" $test_case )

i=0
while [ $i -lt ${#params[@]} ]
do
	mode=${params[$i]}
	i=$(expr $i + 1)
	args=${params[$i]}
	i=$(expr $i + 1)
	case=${params[$i]}
	i=$(expr $i + 1)
	echo "Running test - Mode \"$mode\" Case \"$case\" Args \"$args\""
	pretest
	../../mtmanager/kqmanager -r 1 -n 2 -c $client1 -c $client2 -s $server -f $case -o $result_dir/${mode}.t -e $result_dir/${mode}.r ${args}
	posttest
	echo "Done."
done
