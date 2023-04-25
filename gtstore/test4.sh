#!/bin/bash

# call ./start_service.sh --nodes 7 --rep 3

for i in {1..20}
do
  ./bin/simple_client --put "key$i" --val "val$i"
done

for i in {1..2}
do
  index=$(( $RANDOM % 20 ))
  echo changing $index
  key=key$index

  response=$(./bin/simple_client --put "$key" --val "new_val_$i")

  # Extract the server PID from the response
  server_pid=${response#*,}

  # Kill the server PID
  kill -9 "$server_pid"

  ./bin/simple_client --get "$key"
done
