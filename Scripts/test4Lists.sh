#!/bin/bash

function portable_nproc()
{
    # Get number of cores
    OS=$(uname -s)
    if [ "$OS" = "Linux" ]; then
        NPROCS=$(nproc --all)
    elif [ "$OS" = "Darwin" ] || \
         [ "$(echo $OS | grep -q BSD)" = "BSD" ]; then
        NPROCS=$(sysctl -n hw.ncpu)
    else
        NPROCS=$(getconf _NPROCESSORS_ONLN) # glibc/coreutils fallback
    fi
    echo "$NPROCS"
}

function portable_nsock()
{
    # Get number of sockets
    NSOCK=$(cat /proc/cpuinfo 2>/dev/null | grep "physical id" | sort -u | wc -l)
    if [ $? -ne 0 ] || [ $NSOCK -eq 0 ]; then
	# Assume it is 1 in case of error
        echo "1"
    else
        echo "$NSOCK"
    fi
}

ncpu=$(portable_nproc)
nsock=$(portable_nsock)
ncpu_per_sock=$(( $ncpu / $nsock ))

# Algo
ALGO=("LinkFreeList" "SOFTList")

# Update %
: ${UPDATES:="0 5 10 20 30 40 50"}

# Range
: ${RANGES:="256 1024 4096"}

# Duration (s)
: ${DURATION:=15}

# Threads
: ${THREADS:=$ncpu_per_sock}

# PMEM Directory
: ${PMEM_DIR:=/mnt/pmem1}

# Uncomment to use AVX512f nt-stores
#export PMEM_AVX512F=1

# Uncomment to disable PMEM flush
#export PMEM_NO_FLUSH=1

for algo in ${ALGO[*]}; do
	for range in ${RANGES[*]}; do
		echo -e "Algo: $algo, Range: $range, Threads: $THREADS"
		echo -e "Update %\tThroughput"

		for ur in ${UPDATES[*]}; do
			# Cleanup
			for i in $(seq 1 $THREADS); do
				rm -f $PMEM_DIR/p_llist_$i
			done
			sleep 2

			# Run app
			numactl -N 0 ./list -a $algo -p $THREADS -d $DURATION -R $(( 100 - $ur )) -M $range -t 1 > output.log 2>&1

			# Get throughput
			thr=$(tail -n 1 output.log)
			thr=$(echo "$thr * 1000" | bc)
			echo -e "$ur\t\t$thr"
			sleep 2
		done
		echo -e "\n"
	done
done
