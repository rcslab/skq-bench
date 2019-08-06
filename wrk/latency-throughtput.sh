#!/bin/sh

begin_point=1000
end_point=100000
step=1000
inv_step=-1000

wrk_duration=60
sleep_time=20

wrk_thread=4
connection_num=40

server_addr="skylake3.rcs.uwaterloo.ca"

ssh skylake3 -p 77 "killall httpd"
sleep 1
ssh skylake3 -p 77 "./kqueue_test/httpd -d /usr/local/www/nginx" &
sleep 1
./wrk_test -v -T 1 -S 0.0.0.0 -P 19999 -D /usr/local/www/nginx -t $wrk_thread -d $wrk_duration -c $connection_num -p http://$server_addr:19999/index.html -s wrk_test.lua -o default.csv -H ./httpd -r $begin_point -M
ssh skylake3 -p 77 "killall httpd"

for i in $(seq $(($begin_point+$step)) $step $end_point)
do
        echo "==> Run will throughput = $i"
        ssh skylake3 -p 77 "./kqueue_test/httpd -d /usr/local/www/nginx" &
        sleep $sleep_time
        ./wrk_test -v -T 1 -S 0.0.0.0 -P 19999 -D /usr/local/www/nginx -t $wrk_thread -d $wrk_duration -c $connection_num -p http://$server_addr:19999/index.html -s wrk_test.lua -o default.csv  -H ./httpd -r $i -a -M
        ssh skylake3 -p 77 "killall httpd"
done


for i in $(seq $(($end_point-$step)) $inv_step $begin_point)
do
        echo "==> Run will throughput = $i"
        ssh skylake3 -p 77 "./kqueue_test/httpd -d /usr/local/www/nginx" &
        sleep $sleep_time
        ./wrk_test -v -T 1 -S 0.0.0.0 -P 19999 -D /usr/local/www/nginx -t $wrk_thread -d $wrk_duration -c $connection_num -p http://$server_addr:19999/index.html -s wrk_test.lua -o default.csv -r $i   -H ./httpd -a -M
        ssh skylake3 -p 77 "killall httpd"
done

