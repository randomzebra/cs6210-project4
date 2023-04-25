##!/bin/sh
cd bin &&

./simple-client --put key1 --val value1 &

./client --get key1 &

./client --put key1 --val value2 &

./client --put key2 --val value3 &

./client --put key3 --val value4 &

./client --get key1 &

./client --get key2 &

./client --get key3 &

cd -
