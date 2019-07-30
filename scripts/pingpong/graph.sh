#!/usr/local/bin/bash

# TODO: make this a python script because it's easier

result_dir=results.d
output_dir=outputs.d

echo "Creating directories..."
rm -rf $output_dir
mkdir $output_dir

full_case="123" # random stuff not used
test_case="234" # random stuff not used

# now THIS is a hack
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
	echo "Generating graph for - mode \"$mode\" case \"$case\""
	python3 ../../graph/thread_scale.py $result_dir/${mode}.t $output_dir/${mode}.t.png
done


