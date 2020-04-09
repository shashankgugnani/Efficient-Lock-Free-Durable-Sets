#!/bin/bash

# Algo
ALGO=("LinkFreeList" "SOFTList")

# Update %
UR=("0" "5" "10" "20" "30" "40" "50")

# Duration (s)
DUR=15

# Range
RANGE=4096

# Threads
THREADS=28

# PMEM Directory
PMEM_DIR=/mnt/pmem1

# Uncomment to disable PMEM flush
#export PMEM_NO_FLUSH=1

for algo in ${ALGO[*]}; do
	echo -e "$algo"
	echo -e "Update %\tThroughput"

	for ur in ${UR[*]}; do
		# Cleanup
		for i in $(seq 1 $THREADS); do
			rm -f $PMEM_DIR/p_llist_$i
		done
		sleep 2

		# Run app
		numactl -N 0 ./list -a $algo -p $THREADS -d $DUR -R $(( 100 - $ur )) -M $RANGE -t 1 > output.log 2>&1

		# Get throughput
		thr=$(tail -n 1 output.log)
		thr=$(echo "$thr * 1000" | bc)
		echo -e "$ur\t\t$thr"
		sleep 2
	done
	echo -e "\n"
done
