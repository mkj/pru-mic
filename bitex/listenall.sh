#!/bin/sh

for i in `seq 0 7`; do
    socat TCP-LISTEN:999$i,fork,linger=2 EXEC:"./bitex $i /dev/bulk_samp31" &
done

wait
