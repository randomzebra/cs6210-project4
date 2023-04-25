#!/bin/bash

# Parse command-line arguments
OPTIONS=$(getopt -o '' -l nodes:,rep: -- "$@")
eval set -- "$OPTIONS"

while true; do
  case "$1" in
    --nodes)
      NUM_NODES="$2"
      shift 2
      ;;
    --rep)
      NUM_REP="$2"
      shift 2
      ;;
    --)
      shift
      break
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

# Check if required arguments are set
if [[ -z $NUM_NODES ]]; then
  echo "Missing --nodes argument"
  exit 1
fi

if [[ -z $NUM_REP ]]; then
  echo "Missing --rep argument"
  exit 1
fi

# Spawn manager process
echo "Spawning manager process"
./bin/manager $NUM_NODES $NUM_REP &

# Spawn storage processes
echo "Spawning $NUM_NODES storage processes"
for (( i=0; i<$NUM_NODES; i++ )); do
  ./bin/storage &
done

# Wait for all storage processes to finish
echo "Waiting for storage processes to finish"
wait

# All done!
echo "All processes finished"

