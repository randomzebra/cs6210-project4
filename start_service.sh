#!/bin/bash
NODE=unset
REP=unset

PARSED_ARGUMENTS=$(getopt -a -n start_service -o n:r: --long node:,rep: -- "$@")

VALID_ARGUMENTS=$?
if [ "$VALID_ARGUMENTS" != "0" ]; then
  exit 2
fi
echo "PARSED_ARGUMENTS is $PARSED_ARGUMENTS"
eval set -- "$PARSED_ARGUMENTS"

while :
do
  case "$1" in
    -n | --node) NODE="$2" ; shift 2 ;;
    -r | --rep)   REP="$2"   ; shift 2 ;;
    # -- means the end of the arguments; drop this, and break out of the while loop
    --) shift; break ;;
    # If invalid options were passed, then getopt should have reported an error,
    # which we checked as VALID_ARGUMENTS when getopt was called...
    *) echo "Unexpected option: $1 - this should not happen."
       usage ;;
  esac
done
echo "Starting manager"
./gtstore/bin/manager $NODE $REP &
sleep 5;
echo "manager started"
x=0
while [ $x -lt $NODE ]; 
do
    echo "Starting node $x"
    ./gtstore/bin/storage &
    x=$(( $x + 1 ))
done 
