#!/bin/sh
cd lua_delay_updater
make
cp ./lua_delay_updater ../ldu
cd ..
make

i=1
echo "==> Run will delay = $i"
./ldu wrk_test.lua $i
./wrk_test -v -T 1 -S 0.0.0.0 -P 19999 -D /usr/local/www/nginx -t 4 -d 30 -c 100 -p http://localhost:19999/index.html -s wrk_test.lua -o default.csv -H /home/hao/dev/celestis/Celestis/build/tools/httpd/httpd

for i in $(seq 2 1 50)
do
	echo "==> Run will delay = $i"
	./ldu wrk_test.lua $i
	sleep 10
	./wrk_test -v -T 1 -S 0.0.0.0 -P 19999 -D /usr/local/www/nginx -t 4 -d 30 -c 100 -p http://localhost:19999/index.html -s wrk_test.lua -o default.csv -H /home/hao/dev/celestis/Celestis/build/tools/httpd/httpd -a
done


for i in $(seq 49 -1 1)
do
	echo "==> Run will delay = $i"
	./ldu wrk_test.lua $i
	sleep 10
	./wrk_test -v -T 1 -S 0.0.0.0 -P 19999 -D /usr/local/www/nginx -t 4 -d 30 -c 100 -p http://localhost:19999/index.html -s wrk_test.lua -o default.csv -H /home/hao/dev/celestis/Celestis/build/tools/httpd/httpd -a
done

