#!/bin/bash

# Compile code for throughput measurements
make clean
sleep 2
make
sleep 2

# Test lists in ADR mode
echo "ADR Tests"
Scripts/test4Lists.sh
sleep 5

# Test lists in eADR mode
make clean
sleep 2
make
sleep 2
echo "eADR Tests"
PMEM_NO_FLUSH=1 Scripts/test4Lists.sh
sleep 5

# Compile code for latency measurements
make clean
sleep 2
make ENABLE_TIMER=y
sleep 2

# Get list latencies in ADR mode
echo "ADR Latency Tests"
Scripts/test4Lists.sh > _temp
sleep 5
mv output.log adr-latency.log

# Get list latencies in eADR mode
echo "ADR Tests"
PMEM_NO_FLUSH=1 Scripts/test4Lists.sh > _temp
sleep 5
mv output.log eadr-latency.log
rm -f _temp
